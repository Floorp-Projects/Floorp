/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


#ifdef MOZ_USE_NATIVE_UCONV
#include "nsString.h"
#include "nsIGenericFactory.h"

#include "nsINativeUConvService.h"

#include "nsIUnicodeDecoder.h"
#include "nsIUnicodeEncoder.h"
#include "nsICharRepresentable.h"

#include "nsNativeUConvService.h"

#include <nl_types.h> // CODESET
#include <langinfo.h> // nl_langinfo
#include <iconv.h>    // iconv_open, iconv, iconv_close
#include <errno.h>


class IConvAdaptor : public nsIUnicodeDecoder, 
                     public nsIUnicodeEncoder, 
                     public nsICharRepresentable
{
public:
    IConvAdaptor();
    virtual ~IConvAdaptor();
    
    nsresult Init(const char* from, const char* to);
    
    NS_DECL_ISUPPORTS
    
    // Decoder methods:
    
    NS_IMETHOD Convert(const char * aSrc, 
                       PRInt32 * aSrcLength, 
                       PRUnichar * aDest, 
                       PRInt32 * aDestLength);
    
    NS_IMETHOD GetMaxLength(const char * aSrc, 
                            PRInt32 aSrcLength, 
                            PRInt32 * aDestLength);
    NS_IMETHOD Reset();
    
    // Encoder methods:
    
    NS_IMETHOD Convert(const PRUnichar * aSrc, 
                       PRInt32 * aSrcLength, 
                       char * aDest, 
                       PRInt32 * aDestLength);
    
    
    NS_IMETHOD Finish(char * aDest, PRInt32 * aDestLength);
    
    NS_IMETHOD GetMaxLength(const PRUnichar * aSrc, 
                            PRInt32 aSrcLength, 
                            PRInt32 * aDestLength);
    
    // defined by the Decoder:  NS_IMETHOD Reset();
    
    NS_IMETHOD SetOutputErrorBehavior(PRInt32 aBehavior, 
                                      nsIUnicharEncoder * aEncoder, 
                                      PRUnichar aChar);
    
    NS_IMETHOD FillInfo(PRUint32* aInfo);
    
    
private:
    nsresult ConvertInternal(void * aSrc, 
                             PRInt32 * aSrcLength, 
                             PRInt32 aSrcCharSize,
                             void * aDest, 
                             PRInt32 * aDestLength,
                             PRInt32 aDestCharSize);
    
    
    iconv_t mConverter;
    PRBool    mReplaceOnError;
    PRUnichar mReplaceChar;

#ifdef DEBUG
    nsCString mFrom, mTo;
#endif
};

NS_IMPL_ISUPPORTS3(IConvAdaptor, 
                   nsIUnicodeEncoder, 
                   nsIUnicodeDecoder,
                   nsICharRepresentable)

IConvAdaptor::IConvAdaptor()
{
    mConverter = 0;
    mReplaceOnError = PR_FALSE;
}

IConvAdaptor::~IConvAdaptor()
{
    if (mConverter)
        iconv_close(mConverter);
}

nsresult 
IConvAdaptor::Init(const char* from, const char* to)
{
#ifdef DEBUG
    mFrom = from;
    mTo = to;
#endif

    mConverter = iconv_open(to, from);
    if (mConverter == (iconv_t) -1 )    
    {
#ifdef DEBUG
        printf(" * IConvAdaptor - FAILED Initing: %s ==> %s\n", from, to);
#endif
        mConverter = nsnull;
        return NS_ERROR_FAILURE;
    }
    return NS_OK;
}

// From some charset to ucs2
nsresult 
IConvAdaptor::Convert(const char * aSrc, 
                     PRInt32 * aSrcLength, 
                     PRUnichar * aDest, 
                     PRInt32 * aDestLength)
{
    return ConvertInternal( (void*) aSrc, 
                            aSrcLength, 
                            1,
                            (void*) aDest, 
                            aDestLength,
                            2);
}

nsresult
IConvAdaptor::GetMaxLength(const char * aSrc, 
                          PRInt32 aSrcLength, 
                          PRInt32 * aDestLength)
{
    if (!mConverter)
        return NS_ERROR_UENC_NOMAPPING;

    *aDestLength = aSrcLength*4; // sick
#ifdef DEBUG
    printf(" * IConvAdaptor - - GetMaxLength %d ( %s -> %s )\n", *aDestLength, mFrom.get(), mTo.get());
#endif
    return NS_OK;
}


nsresult 
IConvAdaptor::Reset()
{
    const char *zero_char_in_ptr  = NULL;
    char       *zero_char_out_ptr = NULL;
    size_t      zero_size_in      = 0,
                zero_size_out     = 0;

    iconv(mConverter, 
          (char **)&zero_char_in_ptr,
          &zero_size_in,
          &zero_char_out_ptr,
          &zero_size_out);

#ifdef DEBUG
    printf(" * IConvAdaptor - - Reset\n");
#endif
    return NS_OK;
}


// convert unicode data into some charset.
nsresult 
IConvAdaptor::Convert(const PRUnichar * aSrc, 
                     PRInt32 * aSrcLength, 
                     char * aDest, 
                     PRInt32 * aDestLength)
{
    return ConvertInternal( (void*) aSrc, 
                            aSrcLength, 
                            2,
                            (void*) aDest, 
                            aDestLength,
                            1);
}


nsresult 
IConvAdaptor::Finish(char * aDest, PRInt32 * aDestLength)
{
    *aDestLength = 0;
    return NS_OK;
}

nsresult 
IConvAdaptor::GetMaxLength(const PRUnichar * aSrc, 
                          PRInt32 aSrcLength, 
                          PRInt32 * aDestLength)
{
    if (!mConverter)
        return NS_ERROR_UENC_NOMAPPING;

    *aDestLength = aSrcLength*4; // sick

    return NS_OK;
}


nsresult 
IConvAdaptor::SetOutputErrorBehavior(PRInt32 aBehavior, 
                                    nsIUnicharEncoder * aEncoder, 
                                    PRUnichar aChar)
{
    if (aBehavior == kOnError_Signal) {
        mReplaceOnError = PR_FALSE;
        return NS_OK;
    }
    else if (aBehavior != kOnError_Replace) {
        mReplaceOnError = PR_TRUE;
        mReplaceChar = aChar;
        return NS_OK;
    }

    NS_WARNING("Uconv Error Behavior not support");
    return NS_ERROR_FAILURE;
}

nsresult 
IConvAdaptor::FillInfo(PRUint32* aInfo)
{
#ifdef DEBUG
    printf(" * IConvAdaptor - FillInfo called\n");
#endif
    *aInfo = 0;
    return NS_OK;
}


nsresult 
IConvAdaptor::ConvertInternal(void * aSrc, 
                             PRInt32 * aSrcLength, 
                             PRInt32 aSrcCharSize,
                             void * aDest, 
                             PRInt32 * aDestLength,
                             PRInt32 aDestCharSize)
{
    if (!mConverter) {
        NS_WARNING("Converter Not Initialize");
        return NS_ERROR_NOT_INITIALIZED;
    }
    size_t res = 0;
    size_t inLeft = (size_t) *aSrcLength * aSrcCharSize;
    size_t outLeft = (size_t) *aDestLength * aDestCharSize;
    size_t outputAvail = outLeft;

    while (true){

        res = iconv(mConverter, 
                    (char**)&aSrc, 
                    &inLeft, 
                    (char**)&aDest, 
                    &outLeft);
        
        if (res == (size_t) -1) {
            // on some platforms (e.g., linux) iconv will fail with
            // E2BIG if it cannot convert _all_ of its input.  it'll
            // still adjust all of the in/out params correctly, so we
            // can ignore this error.  the assumption is that we will
            // be called again to complete the conversion.
            if ((errno == E2BIG) && (outLeft < outputAvail)) {
                res = 0;
                break;
            }
            
            if (errno == EILSEQ) {

                if (mReplaceOnError) {
                    if (aDestCharSize == 1) {
                        (*(char*)aDest) = (char)mReplaceChar;
                        aDest = (char*)aDest + sizeof(char);
                    }
                    else
                    {
                        (*(PRUnichar*)aDest) = (PRUnichar)mReplaceChar;
                        aDest = (PRUnichar*)aDest + sizeof(PRUnichar);
                    
                    }
                    inLeft -= aSrcCharSize;
                    outLeft -= aDestCharSize;

#ifdef DEBUG
                    printf(" * IConvAdaptor - replacing char in output  ( %s -> %s )\n", 
                           mFrom.get(), mTo.get());

#endif
                    res = 0;
                }
            }

            if (res == -1) {
#ifdef DEBUG
                printf(" * IConvAdaptor - Bad input ( %s -> %s )\n", mFrom.get(), mTo.get());
#endif
                return NS_ERROR_UENC_NOMAPPING;
            }
        }

        if (inLeft <= 0 || outLeft <= 0 || res == -1)
            break;
    }


    if (res != (size_t) -1) {

        // xp_iconv deals with how much is remaining in a given buffer
        // but what uconv wants how much we read/written already.  So
        // we fix it up here.
        *aSrcLength  -= (inLeft  / aSrcCharSize);
        *aDestLength -= (outLeft / aDestCharSize);
        return NS_OK;
    }
    
#ifdef DEBUG
    printf(" * IConvAdaptor - - xp_iconv error( %s -> %s )\n", mFrom.get(), mTo.get());
#endif
    Reset();    
    return NS_ERROR_UENC_NOMAPPING;
}

NS_IMPL_ISUPPORTS1(NativeUConvService, nsINativeUConvService)

NS_IMETHODIMP 
NativeUConvService::GetNativeConverter(const char* from,
                                       const char* to,
                                       nsISupports** aResult) 
{
    *aResult = nsnull;

    nsRefPtr<IConvAdaptor> ucl = new IConvAdaptor();
    if (!ucl)
        return NS_ERROR_OUT_OF_MEMORY;

    nsresult rv = ucl->Init(from, to);
    if (NS_SUCCEEDED(rv))
        NS_ADDREF(*aResult = ucl);

    return rv;
}
#endif
