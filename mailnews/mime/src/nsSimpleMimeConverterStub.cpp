/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is simple MIME converter stubs.
 *
 * The Initial Developer of the Original Code is
 * Oracle Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "mimecth.h"
#include "mimeobj.h"
#include "mimetext.h"
#include "mimecom.h"
#include "nsString.h"
#include "nsReadableUtils.h"
#include "nsCategoryManagerUtils.h"
#include "nsCRT.h"
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsISimpleMimeConverter.h"

typedef struct MimeSimpleStub MimeSimpleStub;
typedef struct MimeSimpleStubClass MimeSimpleStubClass;

struct MimeSimpleStubClass {
    MimeInlineTextClass text;
};

struct MimeSimpleStub {
    MimeInlineText text;
    nsCString *buffer;
    nsCOMPtr<nsISimpleMimeConverter> innerScriptable;
};

MimeDefClass(MimeSimpleStub, MimeSimpleStubClass, mimeSimpleStubClass, NULL);

static int
BeginGather(MimeObject *obj)
{
    MimeSimpleStub *ssobj = (MimeSimpleStub *)obj;
    int status = ((MimeObjectClass *)XPCOM_GetmimeLeafClass())->parse_begin(obj);

    if (status < 0)
        return status;

    if (!obj->output_p ||
        !obj->options ||
        !obj->options->write_html_p) {
        return 0;
    }

    ssobj->buffer->Truncate();
    return 0;
}

static int
GatherLine(char *line, PRInt32 length, MimeObject *obj)
{
    MimeSimpleStub *ssobj = (MimeSimpleStub *)obj;

    if (!obj->output_p ||
        !obj->options ||
        !obj->options->output_fn) {
        return 0;
    }
    
    if (!obj->options->write_html_p)
        return MimeObject_write(obj, line, length, PR_TRUE);

    ssobj->buffer->Append(line);
    return 0;
}

static int
EndGather(MimeObject *obj, PRBool abort_p)
{
    MimeSimpleStub *ssobj = (MimeSimpleStub *)obj;

    if (obj->closed_p)
        return 0;
    
    int status = ((MimeObjectClass *)MIME_GetmimeInlineTextClass())->parse_eof(obj, abort_p);
    if (status < 0)
        return status;

    if (ssobj->buffer->IsEmpty())
        return 0;
    
    nsCString asHTML;
    nsresult rv = ssobj->innerScriptable->ConvertToHTML(nsDependentCString(obj->content_type),
                                                        *ssobj->buffer,
                                                        asHTML);
    if (NS_FAILED(rv)) {
        NS_ASSERTION(NS_SUCCEEDED(rv), "converter failure");
        return -1;
    }

    // MimeObject_write wants a non-const string for some reason, but it doesn't mutate it
    status = MimeObject_write(obj, (char *)PromiseFlatCString(asHTML).get(),
                              asHTML.Length(), PR_TRUE);
    if (status < 0)
        return status;
    return 0;
}

static int
Initialize(MimeObject *obj)
{
    MimeSimpleStub *ssobj = (MimeSimpleStub *)obj;
    ssobj->innerScriptable =
        do_CreateInstanceFromCategory(NS_SIMPLEMIMECONVERTERS_CATEGORY, obj->content_type);
    if (!ssobj->innerScriptable)
        return -1;
    ssobj->buffer = new nsCString();
    return 0;
}

static void
Finalize(MimeObject *obj)
{
    MimeSimpleStub *ssobj = (MimeSimpleStub *)obj;
    ssobj->innerScriptable = 0;
    delete ssobj->buffer;
}

static int
MimeSimpleStubClassInitialize(MimeSimpleStubClass *clazz)
{
    MimeObjectClass *oclass = (MimeObjectClass *)clazz;
    oclass->parse_begin = BeginGather;
    oclass->parse_line = GatherLine;
    oclass->parse_eof = EndGather;
    oclass->initialize = Initialize;
    oclass->finalize = Finalize;
    return 0;
}

class nsSimpleMimeConverterStub : public nsIMimeContentTypeHandler
{
public:
    nsSimpleMimeConverterStub(const char *aContentType) : mContentType(aContentType) { }
    virtual ~nsSimpleMimeConverterStub() { }

    NS_DECL_ISUPPORTS

    NS_IMETHOD GetContentType(char **contentType)
    {
        *contentType = ToNewCString(mContentType);
        return *contentType ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
    }
    NS_IMETHOD CreateContentTypeHandlerClass(const char *contentType,
                                             contentTypeHandlerInitStruct *initString,
                                             MimeObjectClass **objClass);
private:
    nsCString mContentType;
};

NS_IMPL_ISUPPORTS1(nsSimpleMimeConverterStub, nsIMimeContentTypeHandler)

NS_IMETHODIMP
nsSimpleMimeConverterStub::CreateContentTypeHandlerClass(const char *contentType,
                                                     contentTypeHandlerInitStruct *initStruct,
                                                         MimeObjectClass **objClass)
{
    *objClass = (MimeObjectClass *)&mimeSimpleStubClass;
    (*objClass)->superclass = (MimeObjectClass *)XPCOM_GetmimeInlineTextClass();
    if (!(*objClass)->superclass)
        return NS_ERROR_UNEXPECTED;;

    initStruct->force_inline_display = PR_TRUE;
    return NS_OK;;
}

nsresult
MIME_NewSimpleMimeConverterStub(const char *aContentType,
                                nsIMimeContentTypeHandler **aResult)
{
    nsRefPtr<nsSimpleMimeConverterStub> inst = new nsSimpleMimeConverterStub(aContentType);
    if (!inst)
        return NS_ERROR_OUT_OF_MEMORY;
    return CallQueryInterface(inst.get(), aResult);
}
