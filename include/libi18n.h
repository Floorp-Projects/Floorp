/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */


/* libi18n.h */


#ifndef INTL_LIBI18N_H
#define INTL_LIBI18N_H

#include "xp.h"
#ifndef iDocumentContext
#define iDocumentContext MWContext *
#endif
#define Stream NET_StreamClass
#define URL URL_Struct
#include "csid.h"

#ifdef _UNICVT_DLL_

#ifdef XP_WIN32
#define UNICVTAPI __declspec(dllexport)

#elif defined(MOZ_STRIP_NOT_EXPORTED)
#define UNICVTAPI __attribute__ ((dllexport))

#else
#define UNICVTAPI
#endif

#else /* _UNICVT_DLL is undefined */
#define UNICVTAPI
#endif

/* Enum for INTL_CSIDIteratorCreate */
enum {
	csiditerate_TryIMAP4Search = 1
};



/*
 * To be called when backend catches charset info on <meta ... charset=...> tag.
 * This will force netlib to go get fresh data again either through cache or
 * network.
 */
enum
{
	METACHARSET_NONE = 0,
	METACHARSET_HASCHARSET,
	METACHARSET_REQUESTRELAYOUT,
	METACHARSET_FORCERELAYOUT,
	METACHARSET_RELAYOUTDONE
};

XP_BEGIN_PROTOS


/*=======================================================*/
/* Character Code Conversion (CCC).
 *
 *
 * CCCDataObject accessor functions are
 * build as a table to allow access from a DLL
 *
 * Note: new functions must be added at the end
 *       or old apps using the new dll will fail
 */
/**@name Character Code Conversion (CCC) */
/*@{*/

/**
 * Function Prototype for the codeset conversion function.
 * 
 * @param obj Specifies the converter object
 * @param src Specifies the text to be converted
 * @param srclen Specifies the length of src
 * @return the converted text. The length of the converted result could be 
 *         access via INTL_GetCCCLen(obj) 
 * @see INTL_GetCCCLen
 * @see INTL_SetCCCCvtfunc
 * 
 */
typedef unsigned char *(*CCCFunc)(CCCDataObject obj, const unsigned char * src, int32 srclen);

/**
 * Function Prototype for the Report Auto Detect Result function.
 * 
 * @param closure Specifies the closure which associated with the converter 
 *                object by calling INTL_SetCCCReportAutoDetect
 * @param obj Specifies the converter object
 * @param doc_csid Specifies the auto-detected document csid
 * @see INTL_SetCCCReportAutoDetect
 * 
 */
typedef void (*CCCRADFunc)(void * closure, CCCDataObject obj, uint16 doc_csid);

/**
 * Opaque converter object. 
 *
 * This struct is an opaque converter object.
 */
struct OpaqueCCCDataObject { /* WARNING: MUST MATCH REAL STRUCT */
        /** pointer to the converter object private functions struct */
	struct INTL_CCCFuncs *funcs_pointer;
};

/**
 * This structure hold the private functions of a conversion object.
 * 
 * <B>WARNING: THIS STRUCT AND THE TABLE MUST BE IN SYNC WITH EACH OTHER </B>
 */
struct INTL_CCCFuncs {
    /** The private function of INTL_SetCCCReportAutoDetect. */
    void           (*set_report_autodetect)(CCCDataObject, CCCRADFunc, void*); 
    /** The private function of INTL_CallCCCReportAutoDetect. */
    void           (*call_report_autodetect)(CCCDataObject, uint16);
    /** The private function of INTL_SetCCCCvtfunc. */
    void           (*set_cvtfunc)(CCCDataObject, CCCFunc);
    /** The private function of INTL_GetCCCCvtfunc. */
    CCCFunc        (*get_cvtfunc)(CCCDataObject);
    /** The private function of INTL_SetCCCJismode. */
    void           (*set_jismode)(CCCDataObject,int32);
    /** The private function of INTL_GetCCCJismode. */
    int32          (*get_jismode)(CCCDataObject);
    /** The private function of INTL_SetCCCCvtflag. */
    void           (*set_cvtflag)(CCCDataObject,int32);
    /** The private function of INTL_GetCCCCvtflag. */
    int32          (*get_cvtflag)(CCCDataObject);
    /** The private function of INTL_GetCCCUncvtbuf. */
    unsigned char* (*get_uncvtbuf)(CCCDataObject);
    /** The private function of INTL_SetCCCDefaultCSID. */
    void           (*set_default_doc_csid)(CCCDataObject, uint16);
    /** The private function of INTL_GetCCCDefaultCSID. */
    uint16         (*get_default_doc_csid)(CCCDataObject);
    /** The private function of INTL_SetCCCFromCSID. */
    void           (*set_from_csid)(CCCDataObject, uint16);
    /** The private function of INTL_GetCCCFromCSID. */
    uint16         (*get_from_csid)(CCCDataObject);
    /** The private function of INTL_SetCCCToCSID. */
    void           (*set_to_csid)(CCCDataObject, uint16);
    /** The private function of INTL_GetCCCToCSID. */
    uint16         (*get_to_csid)(CCCDataObject);
    /** The private function of INTL_SetCCCRetval. */
    void           (*set_retval)(CCCDataObject, int);
    /** The private function of INTL_GetCCCRetval. */
    int            (*get_retval)(CCCDataObject);
    /** The private function of INTL_SetCCCLen. */
    void           (*set_len)(CCCDataObject, int32);
    /** The private function of INTL_GetCCCLen. */
    int32          (*get_len)(CCCDataObject);
};

/**
 * Create and initialize Character Code Converter Object.
 *
 * Create and initialize character code converter.
 * It also set up a converter if a doc_csid is known (by DOC_CSID_KNOWN).
 * Caller is responsible for deallocation of an allocated memory.
 * 
 * @param     c    Pointer to an i18n private data structure.
 * @param     default_doc_csid    Default doc_csid to be used.
 * @return    CCCDataObject Created character code converter object pointer.
 */
PUBLIC CCCDataObject INTL_CreateDocumentCCC(
    INTL_CharSetInfo c,
    uint16 default_doc_csid
);

/**
 * Look for a converter from one charset to another.
 *
 * If the from_csid is CS_DEFAULT, this function uses the ID returned by
 * INTL_GetCCCDefaultCSID. If the to_csid is zero, this function uses the ID
 * returned by INTL_DocToWinCharSetID for the from_csid determined above.
 * If found, the converter function is stored in the given character code
 * conversion object.
 *
 * @param from_csid  Specifies the charset ID to convert from
 * @param to_csid    Specifies the charset ID to convert to
 * @param obj        Specifies the character code converter object
 * @return 1 for success, 0 for failure
 * @see INTL_CreateCharCodeConverter, INTL_CallCharCodeConverter
 */
PUBLIC int INTL_GetCharCodeConverter(
    int16 from_csid,
    int16 to_csid,
    CCCDataObject obj
);

/**
 * Set up charset internal data by meta charset.
 *
 * Given a charset name, this will set up i18n private charset info
 * which is obtained by a given context.
 * Input charset name should be obtained from HTML META tag.
 * 
 * @param     context    Context to be set up.
 * @param     charset_tag    Charset name as an input (e.g. iso-8859-1).
 * @see       INTL_CSIReportMetaCharsetTag
 */
PUBLIC void INTL_CCCReportMetaCharsetTag(
    MWContext *context, 
    char *charset_tag
);

/**
 * Passes some more text to the character code converter.
 *
 * The character code converter object keeps track of the current state as it
 * receives data to convert. If partial characters are received, they are
 * buffered until this function is called again.
 * INTL_GetCharCodeConverter must first be called before calling this function. 
 *
 * In some cases, the text is converted in place (in the input buffer).
 *
 * @param obj  Specifies the character code converter object
 * @param str  Specifies the text to be converted
 * @param len  Specifies the length in bytes of the text
 * @return The converted text, null terminated
 * @see INTL_GetCharCodeConverter
 */
PUBLIC unsigned char *INTL_CallCharCodeConverter(
    CCCDataObject obj,
    const unsigned char *str,
    int32 len
);

/**
 * Initialize and set up a character code converter for a mail charset.
 *
 * Allocate memory and initialize for character code converter.
 * From/To charset is determined by given context or by parsing the source
 * buffer in case of HTML.
 * After charsets are determined, it set up a converter function.
 * Caller is responsible for deallocation of an allocated memory.
 * 
 * @param     context    Context to access charset info.
 * @param     isHTML    If TRUE then the input stream is parsed for meta tag. 
 * @param     buffer   Source buffer.
 * @param     buffer_size   the length of the source buffer.
 * @return    CCCDataObject Created character code converter object pointer.
 * @see       INTL_CreateCharCodeConverter
 */
PUBLIC CCCDataObject INTL_CreateDocToMailConverter(
    iDocumentContext context, 
    XP_Bool isHTML, 
    unsigned char *buffer, 
    uint32 buffer_size
);

/**
 * Create a character code converter object used for codeset conversion.
 *
 * @return The new character code converter object
 * @see INTL_CreateDocumentCCC, INTL_GetCharCodeConverter,
 *      INTL_DestroyCharCodeConverter
 * @deprecated Obsolescent. Please use INTL_CreateDocumentCCC. 
 */
PUBLIC CCCDataObject INTL_CreateCharCodeConverter(void);

/**
 * Frees the given character code conversion object.
 *
 * This function destroys the code conversion object created by 
 * INTL_CreateCharCodeConverter.
 *
 * @param obj  Specifies the character code conversion object to free
 * @see INTL_CreateCharCodeConverter
 */
PUBLIC void INTL_DestroyCharCodeConverter(
    CCCDataObject obj
);

/**
 * Converts a piece of text from one charset to another.
 *
 * This function does not do charset ID auto-detection. The caller must pass
 * the from/to charset IDs. This function does not keep state. Don't use it to
 * convert a stream of data. Only use this when you want to convert a string,
 * and you have no way to hold on to the converter object.
 *
 * If the string gets converted in place (use the input buffer), then this
 * function returns NULL.
 *
 * @param fromcsid    Specifies the charset ID to convert from
 * @param tocsid      Specifies the charset ID to convert to
 * @param pSrc        Specifies the input text
 * @param block_size  Specifies the number of bytes in the input text
 * @return The converted text, null terminated, or NULL if converted in place
 * @see INTL_CallCharCodeConverter
 */
PUBLIC unsigned char *INTL_ConvertLineWithoutAutoDetect(
    int16 fromcsid,
    int16 tocsid,
    unsigned char *pSrc,
    uint32 block_size
);

/**
 * Returns the window charset ID corresponding to the given document charset ID.
 *
 * This function searches a built-in table to find the first entry that
 * matches the given document charset ID. If no such entry is found, it
 * returns CS_FE_ASCII.
 *
 * @param csid  Specifies the document charset ID
 * @return The corresponding window charset ID
 */
PUBLIC int16 INTL_DocToWinCharSetID(
    int16 csid
);

/**
 * Return the charset used in internet message from a specified charset.
 *
 * In the current implementation of Communicator, we assume there is a many to
 * one relationship between a encoding and a encoding used on internet mail
 * message. This routines is used to get the outgoing encoding for a specified
 * encoding. The caller than can convert the text of the specified encoding to
 * the return encoding and before send out the internet message. Usually the
 * relationship is the same as the newsgroup posting and this one. However, for
 * some region/country like Korean, it is not the same. In such region/country,
 * they use different encodings in internet mail message and newsgroup posting.
 * In that case INTL_DefaultNewsCharSetID should be used instead.
 *
 * Issues: The current model assume the text of a particular encoding is always
 * sending out as one encoding. Such assumption break when people want send out
 * message in different Cyrillic, Chinese, or Unicode encoding. Therefore, we
 * may change this architecture in the near future.
 *
 * The mapping are: 
 *   <UL>
 *   <LI>CS_ASCII: CS_ASCII 
 *   <LI>CS_LATIN1: CS_LATIN1 
 *   <LI>CS_JIS: CS_JIS 
 *   <LI>CS_SJIS: CS_JIS 
 *   <LI>CS_EUCJP: CS_JIS 
 *   <LI>CS_JIS_AUTO: CS_JIS 
 *   <LI>CS_SJIS_AUTO: CS_JIS 
 *   <LI>CS_EUCJP_AUTO: CS_JIS 
 *   <LI>CS_KSC_8BIT: CS_2022_KR [Note 1]
 *   <LI>CS_KSC_8BIT_AUTO: CS_2022_KR [Note 1]
 *   <LI>CS_GB_8BIT: CS_GB_8BIT 
 *   <LI>CS_BIG5: CS_BIG5 
 *   <LI>CS_CNS_8BIT: CS_BIG5 
 *   <LI>CS_MAC_ROMAN: CS_LATIN1 
 *   <LI>CS_LATIN2: CS_LATIN2 
 *   <LI>CS_MAC_CE,: CS_LATIN2 
 *   <LI>CS_CP_1250: CS_LATIN2 
 *   <LI>CS_8859_5: CS_KOI8_R [Note 2]
 *   <LI>CS_KOI8_R: CS_KOI8_R [Note 2] 
 *   <LI>CS_MAC_CYRILLIC: CS_KOI8_R  [Note 2]
 *   <LI>CS_CP_1251:  CS_KOI8_R [Note 2]
 *   <LI>CS_8859_7: CS_8859_7 
 *   <LI>CS_CP_1253: CS_8859_7 
 *   <LI>CS_MAC_GREEK: CS_8859_7 
 *   <LI>CS_8859_9: CS_8859_9 
 *   <LI>CS_MAC_TURKISH: CS_8859_9 
 *   <LI>CS_UTF8: CS_UTF7 
 *   <LI>CS_UTF7: CS_UTF7 
 *   <LI>CS_UCS2: CS_UTF7 
 *   <LI>CS_UCS2_SWAP: CS_UTF7 
 *   </UL>
 *   Note:
 *   <OL>
 *   <LI>For INTL_DefaultNewsCharSetID, this value is different
 *   <LI>The value is the one specified in preference
 *       "intl.mailcharset.cyrillic". The default value is CS_KOI_R. See
 *       <A HREF=http://people.netscape.com/ftang/cyrillicmail.html>
 *       http://people.netscape.com/ftang/cyrillicmail.html</A> for details.
 *   </OL>
 *
 * @param Specifies the encoding
 * @return the encoding should be send out for the internet mail message.
 * @see INTL_DefaultNewsCharSetID
 */
PUBLIC int16 INTL_DefaultMailCharSetID(int16 csid);

/**
 * Return the charset used in internet message from a specified charset.
 *
 * In the current implementation of Communicator, we assume there is a many to
 * one relationship between a encoding and a encoding used on internet
 * newsgroup posting. This routines is used to get the outgoing encoding for a
 * specified encoding. The caller than can convert the text of the specified
 * encoding to the return encoding and before post the message to the
 * newsgroup. Usually the relationship is the same as the newsgroup posting
 * and this one. However, for some region/country like Korean, it is not the
 * same. In such region/country, they use different encodings in internet mail
 * message and newsgroup posting. In that case INTL_DefaultMailCharSetID should
 * be used instead.
 *
 * Issues: The current model assume the text of a particular encoding is always
 * sending out as one encoding. Such assumption break when people want send out
 * message in different Cyrillic, Chinese, or Unicode encoding. Therefore, we
 * may change this architecture in the near future.
 *
 * The mapping are:
 *  <UL>
 *  <LI>ASCII: CS_ASCII 
 *  <LI>LATIN1: CS_LATIN1 
 *  <LI>JIS: CS_JIS 
 *  <LI>SJIS: CS_JIS 
 *  <LI>EUCJP: CS_JIS 
 *  <LI>JIS_AUTO: CS_JIS 
 *  <LI>SJIS_AUTO: CS_JIS 
 *  <LI>EUCJP_AUTO: CS_JIS 
 *  <LI>KSC_8BIT: CS_KSC_8BIT [Note 1]
 *  <LI>KSC_8BIT_AUTO: CS_KSC_8BIT [Note 1]
 *  <LI>GB_8BIT: CS_GB_8BIT 
 *  <LI>BIG5: CS_BIG5 
 *  <LI>CNS_8BIT: CS_BIG5 
 *  <LI>MAC_ROMAN: CS_LATIN1 
 *  <LI>LATIN2: CS_LATIN2 
 *  <LI>MAC_CE,: CS_LATIN2 
 *  <LI>CP_1250: CS_LATIN2 
 *  <LI>8859_5: CS_KOI8_R [Note 2]
 *  <LI>KOI8_R: CS_KOI8_R [Note 2] 
 *  <LI>MAC_CYRILLIC: CS_KOI8_R  [Note 2]
 *  <LI>CP_1251:  CS_KOI8_R [Note 2]
 *  <LI>8859_7: CS_8859_7 
 *  <LI>CP_1253: CS_8859_7 
 *  <LI>MAC_GREEK: CS_8859_7 
 *  <LI>8859_9: CS_8859_9 
 *  <LI>MAC_TURKISH: CS_8859_9 
 *  <LI>UTF8: CS_UTF7 
 *  <LI>UTF7: CS_UTF7
 *  <LI>UCS2: CS_UTF7
 *  <LI>UCS2_SWAP: CS_UTF7 
 *  </UL>
 *  Note:
 *  <OL>   
 *  <LI>For INTL_DefaultMailCharSetID, this value is different
 *  <LI>The value is the one specified in preference
 *      "intl.mailcharset.cyrillic". The default value is CS_KOI_R. See
 *      <A HREF=http://people.netscape.com/ftang/cyrillicmail.html>
 *      http://people.netscape.com/ftang/cyrillicmail.html</A> for details.
 *  </OL>
 *
 * @param Specifies the encoding
 * @return the encoding should be send out for the internet newsgroup.
 * @see INTL_DefaultMailCharSetID
 */
PUBLIC int16 INTL_DefaultNewsCharSetID(int16 csid);

/**
 * Tell libi18n which font charset IDs are available in the front end.
 *
 * The front end (FE) calls this function to inform libi18n of the charset IDs
 * of the fonts that are currently available.
 *
 * This function calls INTL_SetUnicodeCSIDList to set up the Unicode
 * machinery.
 *
 * The front end must allocate space for this array using malloc/calloc. If
 * this function is called more than once, the array passed in a previous call
 * is freed by this function. However, the front end is responsible for
 * freeing the array at exit time.
 *
 * @param charsets  Specifies a null-terminated array of charset IDs
 */
PUBLIC void INTL_ReportFontCharSets(
    int16 *charsets
);

/**
 * Get the "Unconverted Buffer" from the Converter Object.
 * 
 * @param obj Specifies the converter object
 * @return the unconverted buffer in the converter object
 */
#define INTL_GetCCCUncvtbuf(obj) (obj->funcs_pointer->get_uncvtbuf)(obj)

/**
 * Set the "conversion result length" to the converter object.
 * 
 * @param obj Specifies the converter object
 * @param len Specifies the length of current conversion result.
 * @see INTLGetCCCLen
 */
#define INTL_SetCCCLen(obj,len) ((obj)->funcs_pointer->set_len)((obj), (len))

/**
 * Get the "conversion result length" from the converter object.
 * 
 * @param obj Specifies the converter object
 * @return the length of conversion result stored in the converter object
 * @see INTL_SetCCCLen
 */
#define INTL_GetCCCLen(obj) ((obj)->funcs_pointer->get_len)(obj)

/**
 * Set a private flag "Jismode" to the converter object.
 * 
 * There are no reason any code outside libi18n should call this. 
 * We are considering move this into intlpriv.h. 
 * Don't call this macro unless you are changing libi18n.
 *
 * The name "jismode" refers to the ISO 2022 state (JIS mode). 
 * This is what the field was first used for. 
 * It is now used for other purposes as well, so the name is no longer
 * appropriate. 
 *
 * @param obj Specifies the converter object
 * @param jismode Specifies the Jismode
 * @see INTL_GetCCCJismode
 */
#define INTL_SetCCCJismode(obj,jismode) \
				((obj)->funcs_pointer->set_jismode)((obj), (jismode))
/**
 * Get a private flag "Jismode" from the converter object.
 * 
 * There are no reason any code outside libi18n should call this. 
 * We are considering move this into intlpriv.h. 
 * Don't call this macro unless you are changing libi18n
 *
 * The name "jismode" refers to the ISO 2022 state (JIS mode). 
 * This is what the field was first used for. 
 * It is now used for other purposes as well, so the name is no longer
 * appropriate. 
 *
 * @param obj Specifies the converter object
 * @return the Jismode stored in the converter object
 * @see INTL_SetCCCJismode
 */
#define INTL_GetCCCJismode(obj) ((obj)->funcs_pointer->get_jismode)(obj)

/**
 * Set a private flag "Cvtflag" to the converter object.
 * 
 * There are no reason any code outside libi18n should call this. 
 * We are considering move this into intlpriv.h. 
 * Don't call this macro unless you are changing libi18n
 *
 * @param obj Specifies the converter object
 * @param cvtflag Specifies the Cvtflag
 * @see INTL_GetCCCCvtflag
 */
#define INTL_SetCCCCvtflag(obj,cvtflag) \
				((obj)->funcs_pointer->set_cvtflag)((obj), (cvtflag))
/**
 * Get a private flag "Cvtflag" from the converter object.
 * 
 * There are no reason any code outside libi18n should call this. 
 * We are considering move this into intlpriv.h. 
 * Don't call this macro unless you are changing libi18n
 *
 * @param obj Specifies the converter object
 * @return the Cvtflag stored in the converter object
 * @see INTL_SetCCCCvtflag
 */
#define INTL_GetCCCCvtflag(obj) ((obj)->funcs_pointer->get_cvtflag)(obj)

/**
 * Set the "Convert To CSID" to the converter object.
 * 
 * There are no reason any code outside libi18n should call this. 
 * We are considering move this into intlpriv.h. 
 * Don't call this macro unless you are changing libi18n
 *
 * @param obj Specifies the converter object
 * @param to_csid Specifies the Convert To CSID
 * @see INTL_SetCCCToCSID
 */
#define INTL_SetCCCToCSID(obj,to_csid) \
			(((obj)->funcs_pointer->set_to_csid)((obj),(to_csid)))
/**
 * Get the "Convert To CSID" from the converter object.
 *
 * @param obj Specifies the converter object
 * @return the "Convert To CSID" stored in the converter object
 * @see INTL_SetCCCToCSID
 */
#define INTL_GetCCCToCSID(obj) (((obj)->funcs_pointer->get_to_csid)(obj))

/**
 * Set the "Convert From CSID" to the converter object.
 * 
 * There are no reason any code outside libi18n should call this. 
 * We are considering move this into intlpriv.h. 
 * Don't call this macro unless you are changing libi18n
 *
 * @param obj Specifies the converter object
 * @param from_csid Specifies the Convert From CSID
 * @see INTL_SetCCCFromCSID
 */
#define INTL_SetCCCFromCSID(obj,from_csid) \
			(((obj)->funcs_pointer->set_from_csid)((obj),(from_csid)))
/**
 * Get the "Convert From CSID" from the converter object.
 *
 * @param obj Specifies the converter object
 * @return the "Convert From CSID" stored in the converter object
 * @see INTL_SetCCCFromCSID
 */
#define INTL_GetCCCFromCSID(obj) (((obj)->funcs_pointer->get_from_csid)(obj))

/**
 * Set the "Return Value" to the converter object.
 * 
 * There are no reason any code outside libi18n should call this. 
 * We are considering move this into intlpriv.h. 
 * Don't call this macro unless you are changing libi18n
 *
 * @param obj Specifies the converter object
 * @param retval Specifies the "Return Value"
 * @see INTL_GetCCCRetval
 */
#define INTL_SetCCCRetval(obj,retval) \
			(((obj)->funcs_pointer->set_retval)((obj),(retval)))
/**
 * Get the "Return Value" from the converter object.
 * 
 * There are no reason any code outside libi18n should call this. 
 * We are considering move this into intlpriv.h. 
 * Don't call this macro unless you are changing libi18n
 *
 * @param obj Specifies the converter object
 * @return  the "Return Value" stored in the converter object
 * @see INTL_SetCCCRetval
 */
#define INTL_GetCCCRetval(obj) (((obj)->funcs_pointer->get_retval)(obj))

/**
 * Set the "Conversion Function" to the converter object.
 * 
 * There are no reason any code outside libi18n should call this. 
 * We are considering move this into intlpriv.h. 
 * Don't call this macro unless you are changing libi18n
 *
 * @param obj Specifies the converter object
 * @param func Specifies the "Conversion Function" stored in the converter
 *             object
 * @see INTL_GetCCCCvtfunc
 */
#define INTL_SetCCCCvtfunc(obj,func) \
				(((obj)->funcs_pointer->set_cvtfunc)((obj),(func)))

/**
 * Get the "Conversion Function" from the converter object.
 * 
 * @param obj Specifies the converter object
 * @return  the "Conversion Function" stored in the converter object
 * @see INTL_SetCCCCvtfunc
 */
#define INTL_GetCCCCvtfunc(obj) ((obj)->funcs_pointer->get_cvtfunc)(obj)

/**
 * Set the "Report Auto Detect Result Function" to the converter object.
 * 
 * @param obj Specifies the converter object
 * @param func Specifies the "Auto Detect Result Reporting Function" 
 * @param closure Specifies the closure which will be pass to the "Auto
 *                Detect Result Reporting Function" 
 * @see INTL_CallCCCReportAutoDetect
 */
#define INTL_SetCCCReportAutoDetect(obj,func,closure) \
    (((obj)->funcs_pointer->set_report_autodetect)((obj), (func), (closure)))

/**
 * Call the "Report Auto Detect Result Function" associated with the 
 * converter object.
 * 
 * @param obj Specifies the converter object
 * @param doc_csid Specifies the document csid which be auto detected
 * @see INTL_CallCCCReportAutoDetect
 */
#define INTL_CallCCCReportAutoDetect(obj,doc_csid) \
		(((obj)->funcs_pointer->call_report_autodetect)((obj), (doc_csid)))

/**
 * Set the "Default Document CSID" to the converter object.
 * 
 * There are no reason any code outside libi18n should call this. 
 * We are considering move this into intlpriv.h. 
 * Don't call this macro unless you are changing libi18n
 *
 * @param obj Specifies the converter object
 * @param default_doc_csid Specifies the Default Document CSID
 * @see  INTL_GetCCCDefaultCSID
 */
#define INTL_SetCCCDefaultCSID(obj,default_doc_csid) \
		((obj)->funcs_pointer->set_default_doc_csid)((obj), (default_doc_csid))

/**
 * Get the "Default Document CSID" from the converter object.
 * 
 * There are no reason any code outside libi18n should call this. 
 * We are considering move this into intlpriv.h. 
 * Don't call this macro unless you are changing libi18n
 *
 * @param obj Specifies the converter object
 * @return  the Default Document CSID stored in the converter object
 * @see  INTL_GetCCCDefaultCSID
 */
#define INTL_GetCCCDefaultCSID(obj) \
		(((obj)->funcs_pointer->get_default_doc_csid)(obj))

/*@}*/
/*=======================================================*/
/**@name CharSetID and Charset Name Mapping */
/*@{*/
/**
 * Returns the preferred MIME charset name corresponding to the given
 * charset ID.
 *
 * Charset names are registered by IANA (Internet Assigned Numbers Authority).
 * The current charset name database can be found at:
 *
 * <A HREF=ftp://ftp.isi.edu/in-notes/iana/assignments/character-sets>
 *         ftp://ftp.isi.edu/in-notes/iana/assignments/character-sets</A>.
 *
 * This function returns the charset name for the given Character Set ID
 * which in most cases corresponds to the "(preferred MIME name)" registered
 * with IANA.  This function may return private names not found in the
 * registered. Private names start with "x-". See INTL_CharSetNameToID for 
 * information about charset IDs.
 *
 * @param charSetID            Specifies the charset ID
 * @param charset_return  Returns the corresponding charset name, max 128 bytes
 * @see INTL_CharSetNameToID
 */
PUBLIC void INTL_CharSetIDToName(
    int16 charSetID,
    char *charset_return
);

/**
 * Returns the charset ID corresponding to the given charset name.
 *
 * The charset ID is a private 16-bit integer, described in
 * ns/include/csid.h. If the given charset is unknown, CS_UNKNOWN is returned.
 * If the given charset is NULL, CS_DEFAULT is returned. Charset names are not
 * case-sensitive. See INTL_CharSetIDToName for a description of charset names.
 *
 * @param charset  Specifies the charset name
 * @return the corresponding charset ID
 * @see INTL_CharSetIDToName
 */
PUBLIC int16 INTL_CharSetNameToID(
    char *charset
);

/**
 * Returns a pointer to the preferred MIME charset name corresponding 
 * to the given charset ID.
 *
 * This function is similar to INTL_CharSetIDToName. It returns a pointer to
 * the charset name. See INTL_CharSetIDToName for other details.
 *
 * @param charSetID  Specifies the charset ID
 * @return The corresponding charset name
 * @see INTL_CharSetIDToName
 */
PUBLIC unsigned char *INTL_CsidToCharsetNamePt(
    int16 charSetID
);

/**
 * Returns the Java charset name corresponding to the given charset ID.
 *
 * The Java charset name is one that JDK 1.1 and up will understand.
 * The Java name is defined in 
 * <A HREF=
 * http://java.sun.com/products/jdk/1.1/docs/guide/intl/intl.doc.html#25303>
 * http://java.sun.com/products/jdk/1.1/docs/guide/intl/intl.doc.html#25303</A>
 *
 * @param charSetID       Specifies the charset ID
 * @param charset_return  Returns the corresponding Java charset name,
 *                        max 128 bytes
 * @see INTL_CharSetIDToJavaCharSetName
 */
PUBLIC void INTL_CharSetIDToJavaName(
    int16 charSetID,
    char *charset_return
);

/**
 * Returns the Java charset name corresponding to the given charset ID.
 *
 * The Java charset name is a name used in JDK 1.1 and up.
 * The Java name is defined in 
 * <A HREF=
 * http://java.sun.com/products/jdk/1.1/docs/guide/intl/intl.doc.html#25303>
 * http://java.sun.com/products/jdk/1.1/docs/guide/intl/intl.doc.html#25303</A>
 *
 * @param charSetID  Specifies the charset ID
 * @return the corresponding Java charset name
 * @see INTL_CharSetIDToJavaName
 */
PUBLIC const char * PR_CALLBACK INTL_CharSetIDToJavaCharSetName(
    int16 charSetID
);

/**
 * Returns a pointer to the Java charset name corresponding to
 * the given charset ID.
 *
 * This function is similar to INTL_CharSetIDToJavaCharSetName. See
 * INTL_CharSetIDToJavaCharSetName for further details.
 *
 * @param charSetID  Specifies the charset ID
 * @return The corresponding Java charset name
 * @see INTL_CharSetIDToJavaCharSetName
 */
PUBLIC unsigned char *INTL_CsidToJavaCharsetNamePt(
    int16 charSetID
);

/*@}*/
/*=======================================================*/
/**@name Character Set Properties */
/*@{*/

/**
 * Returns whether or not auto-detection is available for the given charset ID.
 *
 * For example, this routine will return TRUE for any of the Japanese charset
 * IDs, since a Japanese auto-detection routine is available.
 *
 * @param csid  Specifies the charset ID
 * @return Whether or not auto-detection is available for the charset ID
 * @see INTL_GetCharCodeConverter
 */
PUBLIC XP_Bool INTL_CanAutoSelect(
    int16 csid
);

/**
 * Returns the charset type.
 *
 * Returns the type of the given charset ID. The charset types are defined in
 * csid.h.
 *
 * <UL>
 * <LI>SINGLEBYTE: single-byte charset (e.g. ISO-8859-1, MacRoman)
 * <LI>MULTIBYTE: multi-byte charset (e.g. Shift-JIS, Big5)
 * <LI>STATEFUL: stateful charset (e.g. ISO-2022-JP, UTF-7)
 * <LI>WIDECHAR: wide character charset (e.g. UCS-2, UCS-4)
 * </UL>
 *
 * @param charsetid Specifies the charset ID.
 * @return The charset type. 
 */
#define INTL_CharSetType(charsetid) (charsetid & 0x700)

/*@}*/
/*=======================================================*/
/**@name Finding Character Boundaries */
/*@{*/

/**
 * Returns the number of bytes in the given character.
 *
 * This function checks for zero bytes within the text, returning the actual
 * length even if the preceding byte(s) would normally indicate a longer
 * multibyte character.
 *
 * @param charSetID  Specifies the charset ID of the text
 * @param pstr       Specifies the 1st byte of the character
 * @return The number of bytes in the given character
 * @see INTL_IsLeadByte
 */
PUBLIC int INTL_CharLen(
    int charSetID,
    unsigned char *pstr
);

/**
 * Returns number of bytes in given character, minus 1.
 *
 * This function returns the number of bytes in a character that starts with
 * the given byte, minus 1. I.e. for a single-byte character, it returns zero.
 * For a double-byte character, it returns 1. And so on. Hence, this function
 * returns a non-zero value if the given byte is the "lead byte" of a multibyte
 * character.
 * This function should not be confused with Windows API isleadbyte().
 *
 * @param charSetID  Specifies the charset ID of the text
 * @param ch         Specifies the first byte of a character in the text
 * @return The number of bytes in the given character, minus 1
 * @see INTL_CharLen
 */
PUBLIC int 

PR_CALLBACK 
INTL_IsLeadByte(
    int charSetID,
    unsigned char ch
);

/**
 * Returns a pointer to the 1st byte of the next character.
 *
 * This function checks for zero bytes and returns pstr+1 if any are found,
 * even if the preceding byte(s) would normally indicate a longer character.
 *
 * @param charSetID  Specifies the charset ID of the text
 * @param pstr       Specifies the 1st byte of any previous character
 * @return The 1st byte of the next character
 * @see INTL_CharLen
 */
PUBLIC char *INTL_NextChar(
    int charSetID,
    char *pstr
);

/**
 * Returns the number of the byte pointed to by the given position.
 *
 * Determines whether the byte at the given position is the 1st, 2nd, 3rd
 * or 4th byte of the character at that position. The pstr pointer must point
 * to the first byte of any preceding character in the string. The pos
 * position must be greater than zero, and is the index into pstr plus one.
 * I.e. the byte at pstr[0] has pos 1.
 *
 * If pos points to the only byte in a single-byte character, this function
 * returns zero. Otherwise, if pos points to the 1st byte, it returns 1. If
 * pos points to the 2nd byte, it returns 2. And so on.
 *
 * @param charSetID  Specifies the charset ID of the given text
 * @param pstr       Specifies the beginning of a character in the string
 * @param pos        Specifies the byte position within the string
 * @return The number of the byte at the given position
 * @see INTL_CharLen
 */
PUBLIC int INTL_NthByteOfChar(
    int charSetID,
    char *pstr,
    int pos
);

/**
 * Returns the byte index of the next character.
 *
 * Given the position of a character in some text, this function returns the
 * position of the next character.
 *
 * @param charSetID  Specifies the charset ID of the text
 * @param text  Specifies the beginning of the text
 * @param pos   Specifies the current position within the text
 * @return The position of the next character
 * @see INTL_PrevCharIdxInText
 */
PUBLIC int INTL_NextCharIdxInText(
    int16 charSetID,
    unsigned char *text,
    int pos
);

/**
 * Returns the byte index of the previous character.
 *
 * Given the position of a character in some text, this function returns the
 * position of the previous character.
 *
 * @param charSetID  Specifies the charset ID of the text
 * @param text  Specifies the beginning of the text
 * @param pos   Specifies the current position within the text
 * @return The position of the previous character
 * @see INTL_NextCharIdxInText
 */
PUBLIC int INTL_PrevCharIdxInText(
    int16 charSetID,
    unsigned char *text,
    int pos
);


/**
 * Convert number of bytes to number of characters.
 *
 * Given a number of bytes in a given string, this function determines the
 * number of characters.
 *
 * @param charSetID       Specifies the charset ID of the text
 * @param text       Specifies the text
 * @param byteCount  Specifies the number of bytes
 * @return The number of characters
 * @see INTL_TextCharLenToByteCount
 */
PUBLIC int32 INTL_TextByteCountToCharLen(
    int16 charSetID,
    unsigned char *text,
    uint32 byteCount
);

/**
 * Convert number of characters to number of bytes.
 *
 * Given a number of characters in a given string, this function determines the
 * number of bytes.
 *
 * @param charSetID     Specifies the charset ID of the text
 * @param text     Specifies the text
 * @param charLen  Specifies the number of characters
 * @return The number of bytes
 * @see INTL_TextByteCountToCharLen
 */
PUBLIC int32 INTL_TextCharLenToByteCount(
    int16 charSetID,
    unsigned char *text,
    uint32 charLen
);


/**
 * Returns the byte index of the next character.
 *
 * Given the position of any byte of any character in some text, this function
 * returns the position of the 1st byte of the next character. The
 * difference between this function and INTL_NextCharIdxInText is that this
 * function will accept the position of any byte of a character rather than
 * just the 1st byte of a character.
 *
 * @param charSetID  Specifies the charset ID of the text
 * @param str   Specifies the beginning of the text
 * @param pos   Specifies any byte of any character
 * @return The index of the next character
 * @see INTL_NextCharIdxInText, INTL_PrevCharIdx
 */
PUBLIC int INTL_NextCharIdx(
    int16 charSetID,
    unsigned char *str,
    int pos
);

/**
 * Returns the byte index of the previous character.
 *
 * Given the position of any byte of any character in some text, this function
 * returns the position of the 1st byte of the previous character. The
 * difference between this function and INTL_PrevCharIdxInText is that this
 * function will accept the position of any byte of a character rather than
 * just the 1st byte of a character.
 *
 * @param charSetID  Specifies the charset ID of the text
 * @param str   Specifies the beginning of the text
 * @param pos   Specifies any byte of any character
 * @return The index of the previous character
 * @see INTL_PrevCharIdxInText, INTL_NextCharIdx
 */
PUBLIC int INTL_PrevCharIdx(
    int16 charSetID,
    unsigned char *str,
    int pos
);

/*@}*/
/*=======================================================*/
/**@name Single-Byte Charset Conversion Tables (Obsolescent) */
/*@{*/

/**
 * Free a single-byte charset conversion table.
 *
 * This is not really a public function. However, ns/sun-java/awt/macos needs
 * it, so we have to put it here.
 *
 * @see INTL_GetSingleByteTable
 * @version DEPRECATED. Obsolescent. Use INTL_DestroyCharCodeConverter instead.
 */
MODULE_PRIVATE void INTL_FreeSingleByteTable(char **cvthdl);

/**
 * Get a single-byte charset conversion table.
 *
 * This is not really a public function. However, ns/sun-java/awt/macos needs
 * it, so we have to put it here.
 *
 * @see INTL_FreeSingleByteTable
 * @see INTL_LockTable
 * @version DEPRECATED. Obsolescent. Use INTL_GetCharCodeConverter instead.
 */
MODULE_PRIVATE char **INTL_GetSingleByteTable(
    int16 fromcsid, 
    int16 tocsid,
    int32 func_ctx
);

/**
 * Lock the given single-byte charset conversion table in memory.
 *
 * This is not really a public function. However, ns/sun-java/awt/macos needs
 * it, so we have to put it here.
 *
 * @see INTL_GetSingleByteTable
 * @version DEPRECATED. Obsolescent. See INTL_GetSingleByteTable.
 */
MODULE_PRIVATE char *INTL_LockTable(char **cvthdl);

/*@}*/
/*=======================================================*/
/**@name HTTP Headers */
/*@{*/

/**
 * Return the AcceptLanguage preference.
 *
 * Get the HTTP Accept-Language header from preference settings.
 * 
 * @return    Accept-Language header (null-terminated string).
 * @see       INTL_GetAcceptCharset
 */
PUBLIC char *INTL_GetAcceptLanguage(void); 

/**
 * Return the AcceptCharset preference.
 *
 * Get the HTTP Accept-Charset header from preference settings.
 * 
 * @return    Accept-Charset header (null-terminated string).
 * @see       INTL_GetAcceptLanguage
 */
PUBLIC char *INTL_GetAcceptCharset(void);

/*@}*/
/*=======================================================*/
/**@name Message Header Processing */
/*@{*/

/**
 * Decode and convert message header.
 *
 * This is a convenience macro that calls INTL_DecodeMimePartIIStr. It is
 * similar to INTL_DecodeMimePartIIStr, with the exception that it always
 * attempts to allocate a new buffer instead of returning the original input
 * buffer where the decoding/conversion may have been performed in place.
 *
 * @param r Returns the decoded/converted message header
 * @param b Specifies the message header
 * @param c Specifies the target window charset ID
 * @param f Specifies whether to convert the string into the wincsid or not
 * @return the decoded/converted message header (r)
 * @see INTL_DecodeMimePartIIStr
 */
#define INTL_DECODE_MIME_PART_II(r,b,c,f)  \
	(r = INTL_DecodeMimePartIIStr((b),(c),(f))), \
	((NULL!=r) && ((r)!=(b))) ? r : (r = XP_STRDUP(b))

/**
 * Decode and convert message header.
 *
 * If the message header contains an RFC 2047 encoded-word, that word is
 * decoded. Then it performs charset conversion if the dontConvert parameter is
 * false. Otherwise, it will only decode the string and return. The conversion
 * may happen later in the process. The flag is needed to work around a double
 * conversion problem.
 *
 * @param header       Specifies the message string to be decoded/converted.
 * @param wincsid      Specifies the target window charset ID.
 * @param dontConvert  Specifies whether to convert the string into the wincsid
 *                     or not. If the value is true, then it will only decode 
 *                     any RFC 2047 encoded-words, without converting their
 *                     charsets. If the value is false, then it will decode RFC
 *                     2047 encoded-words AND convert them into the specified
 *                     wincsid.
 * @return Decoded and/or converted message header. If the return value is
 *         different from the input buffer, the caller must free the output
 *         buffer by calling XP_FREE when it is no longer needed.
 * @see INTL_DECODE_MIME_PART_II
 * @see INTL_EncodeMimePartIIStr
 * @see INTL_EncodeMimePartIIStr_VarLen
 */
PUBLIC char *INTL_DecodeMimePartIIStr(
    const char *header, 
    PRInt16 wincsid, 
    PRBool dontConvert
);

/**
 * Convert and encode message header.
 *
 * Convert the string into an encoding used in Internet messages and encode 
 * them as per RFC 2047. It will (1) perform the codeset conversion and 
 * (2) RFC 1522 encoding algorithm (if bUseMime is true or the internet message
 * encoding is ISO-2022-KR or ISO-2022-JP). This is a restrict version of 
 * INTL_EncodeMimePartIIStr_VarLen which always use 72 for encodedWordSize
 *
 * @param    header     Specifies the RFC 1522 string to be encoded.
 * @param    wincsid    Specifies the source encoding
 * @param    bUseMime   Specifies apply RFC 1522 rule or not. If the value is 
 *                      true or the internet message encoding is ISO-2022-JP 
 *                      or ISO-2022-KR, then it perform RFC1522 encoding after
 *                      convert the text into the internet message encoding, 
 *                      Otherwise, it only convert the text into internet
 *                      message encoding.
 * @return   the encoded/converted header. The caller need to free this by
 *           calling XP_FREE when the result is no longer needed.
 * @see      INTL_DecodeMimePartIIStr
 * @see      INTL_EncodeMimePartIIStr_VarLen
 */
PUBLIC char *INTL_EncodeMimePartIIStr(
    char *header, 
    PRInt16 wincsid, 
    PRBool bUseMime
);

/**
 * Convert and encode text into RFC 1522 header.
 *
 * Convert the string into the encoding used in internet message and encode 
 * them into RFC 1522 form. It will (1) perform the codeset conversion and 
 * (2) RFC 1522 encoding algorithm (if bUseMime is true or the internet message
 * encoding is ISO-2022-KR or ISO-2022-JP). It is same as
 * INTL_EncodeMimePartIIStr except it allow encodedWordSize value other than 72.
 *
 * @param header           Specifies the RFC 1522 string to be encoded.
 * @param wincsid          Specifies the source encoding
 * @param bUseMime         Specifies apply RFC 1522 rule or not. If the value
 *                         is true or the internet message encoding is
 *                         ISO-2022-JP or ISO-2022-KR, then it perform RFC1522
 *                         encoding after convert the text into the internet
 *                         message encoding. Otherwise, it only convert the
 *                         text into internet message encoding.
 * @param encodedWordSize  Specifies the maximum length of encoded word.
 * @return the encoded/converted header. The caller need to free this by
 *         calling XP_FREE when the result is no longer needed.
 * @see INTL_DecodeMimePartIIStr
 * @see INTL_EncodeMimePartIIStr
 */
PUBLIC char *INTL_EncodeMimePartIIStr_VarLen(
    char * header, 
    PRInt16 wincsid, 
    PRBool bUseMime, 
    int encodedWordSize
);

/**
 * [OBSOLETE!!!] We should use the INTL_DecodeMimePartIIStr instead of this. 
 * We keep this Macro until we change all the callers.
 * Please do not use this in the future.
 */
#define IntlDecodeMimePartIIStr INTL_DecodeMimePartIIStr

/**
 * [OBSOLETE!!!] We should use the INTL_EncodeMimePartIIStr instead of this. 
 * We keep this Macro until we change all the callers.
 * Please do not use this in the future.
 */
#define IntlEncodeMimePartIIStr INTL_EncodeMimePartIIStr



/**
 * Set a private flag to remember a state mail/news. 
 *
 * A flag is used inside libi18n to remember whether we are sending mail or
 * news. This is because mail encoding and news encoding is different 
 * for Korean.
 * Note that this should be used carefully since it depends on
 * the current mail/news implementation.
 * This is really a hack. It will be removed in the future.
 * 
 * @param toNews     Boolean value to be set to the private flag.
 */
PUBLIC void
INTL_MessageSendToNews(XP_Bool toNews);


/**
 * Convert a string from RFC1522 encoded header and normalize it, by dropping 
 * the case of the character.
 *
 * The return value could be used with INTL_StrContains, INTL_StrIs,
 * INTL_StrBeginWith or INTL_StrEndWith to perform string matching. This
 * function will normalize a string by dropping the case of character according
 * to the csid the caller passed in. It will also ignore CR and LF characters.
 *
 * @param    csid    Specifies the encoding of str
 * @param    str     Specifies the to-be-normalized string.
 * @return   a normalized string which could be used in INTL_StrContains, 
 *           INTL_StrIs , INTL_StrBeginWith and INTL_StrEndWith The caller 
 *           should free it by calling XP_FREE when it is not needed.
 * @see      INTL_GetNormalizeStr
 * @see      INTL_StrContains
 * @see      INTL_StrIs
 * @see      INTL_StrBeginWith
 * @see      INTL_StrEndWith
 */
PUBLIC unsigned char* INTL_GetNormalizeStrFromRFC1522(
    int16 csid, 
    unsigned char* rfc1522header
);


/*@}*/
/*=======================================================*/
/**@name Unicode (UCS-2) Strings */
/*@{*/

/**
 * Unicode character typedef.
 *
 * This is used to represent a 16-bit Unicode (UCS-2) character.
 */
typedef uint16 INTL_Unicode;

/**
 * Return the length of a Unicode string.
 *
 * The given Unicode string must be terminated by U+0000.
 *
 * @param  ustr  Specifies the Unicode string
 * @return The length of ustr in UCS-2 units, not bytes
 */
PUBLIC uint32 INTL_UnicodeLen(INTL_Unicode *ustr);

/*@}*/
/*=======================================================*/
/**@name Compound Strings */
/*@{*/

/**
 * A typedef for encoding IDs (charset IDs).
 *
 * These are equivalent to charset IDs in the current code base.
 */
typedef uint16 INTL_Encoding_ID;

/*
 * See comment below.
 */
typedef struct INTL_CompoundStr INTL_CompoundStr;

/**
 * Compound String.
 * 
 * A Compound String is constructed as a linked list. Each node has two fields
 * and a pointer to the next node. The two fields store a pointer to a
 * uniformly encoded piece of text and the encoding of that text.
 */
struct INTL_CompoundStr {
    /** The encoding of the text in this node. */
    INTL_Encoding_ID encoding;
    /** The uniformly encoded text. */
    unsigned char    *text;
    /** A pointer to the next node. NULL if there are no more nodes. */
    INTL_CompoundStr *next;
};

/**
 * INTL_CompoundStrIterator should really be opaque, but we need to change the
 * callers first. 
 */
typedef INTL_CompoundStr *INTL_CompoundStrIterator; 

/** 
 * Construct an INTL_CompoundStr, given some text and its encoding.
 *
 * Use this with INTL_CompoundStrCat to create multi-encoding
 * INTL_CompoundStrs.
 *
 * @param inencoding  Specifies the encoding of intext.
 * @param intext      Specifies the text to be stored. Null-terminated string.
 * @return INTL_CompoundStr. The caller should use INTL_CompoundStrDestroy to 
 *         destroy it when it is no longer needed.
 * @see INTL_CompoundStrDestroy
 */
PUBLIC INTL_CompoundStr* INTL_CompoundStrFromStr(
    INTL_Encoding_ID inencoding, 
    unsigned char* intext
);

/**
 * Convert the given Unicode string to an INTL_CompoundStr.
 *
 * This routine uses information provided by the front end through
 * INTL_SetUnicodeCSIDList. It converts from Unicode to substrings in the
 * encodings that the front end said were available (in the font system).
 *
 * @param inunicode  Specifies the Unicode text to be converted.
 * @param inlen      Specifies the length of inunicode in UCS-2 units,
 *                   not bytes.
 * @return INTL_CompoundStr. The caller should use INTL_CompoundStrDestroy to 
 *         destroy it when it is no longer needed.
 * @see INTL_CompoundStrDestroy
 */
PUBLIC INTL_CompoundStr* INTL_CompoundStrFromUnicode(
    INTL_Unicode* inunicode, 
    uint32 inlen
);

/**
 * Destroy an INTL_CompoundStr.
 *
 * This function destroys the INTL_CompoundStr created by 
 * INTL_CompoundStrFromStr or INTL_CompoundStrFromUnicode.
 *
 * @param Specifies the INTL_CompoundStr to be destroyed.
 * @see INTL_CompoundStrFromStr
 * @see INTL_CompoundStrFromUnicode
 */
PUBLIC void INTL_CompoundStrDestroy(INTL_CompoundStr* This);

/**
 * Concatenate two INTL_CompoundStrs.
 *
 * @param    s1    Specifies the first INTL_CompoundStr and returns the
 *                 concatenated INTL_CompoundStr
 * @param    s2    Specifies the second INTL_CompoundStr
 * @see      INTL_CompoundStrDestroy
 */
PUBLIC void INTL_CompoundStrCat(
    INTL_CompoundStr* s1, 
    INTL_CompoundStr* s2
);

/**
 * Clone an INTL_CompoundStr.
 *
 * This function clones an INTL_CompoundStr.
 *
 * @param s  Specifies the INTL_CompoundStr to be cloned
 * @return a cloned INTL_CompoundStr. The caller should use
 *         INTL_CompoundStrDestroy to destroy it when it is no longer needed.
 * @see INTL_CompoundStrDestroy
 */
PUBLIC INTL_CompoundStr* INTL_CompoundStrClone(INTL_CompoundStr* s1);

/**
 * Start iterating an INTL_CompoundStr.
 *
 * Initialize the iterating state and perform the first iteration of an
 * INTL_CompoundStr.
 *
 * @param This         Specifies the INTL_CompoundStr to be iterated
 * @param outencoding  Returns the encoding of the first node
 * @param outtext      Returns the text of the first node. The caller should
 *                     not free it.
 * @return INTL_CompoundStrIterator. The state of the iteration. Should be
 *         passed to INTL_CompoundStrNextStr. NULL if the iteration is
 *         finished.
 * @see INTL_CompoundStrNextStr
 */
PUBLIC INTL_CompoundStrIterator INTL_CompoundStrFirstStr(
    INTL_CompoundStr* This, 
    INTL_Encoding_ID *outencoding, 
    unsigned char** outtext
);

/**
 * Iterating INTL_CompoundStr.
 *
 * This function iterates through the INTL_CompoundStr for the given 
 * INTL_CompoundStrIterator.
 *
 * @param    iterator    Specifies the INTL_CompoundStrIterator
 * @param    outencoding    Returns the encoding of the current node
 * @param    outtext    Returns the text of the current node. The caller should 
 *                      not free it.
 * @return INTL_CompoundStrIterator. The state of the iteration. Should be
 *         passed to INTL_CompoundStrNextStr. NULL if the iteration is
 *         finished.
 * @see INTL_CompoundStrFirstStr
 */
PUBLIC INTL_CompoundStrIterator INTL_CompoundStrNextStr(
    INTL_CompoundStrIterator iterator, 
    INTL_Encoding_ID *outencoding, 
    unsigned char** outtext
);

/*@}*/
/*=======================================================*/
/**@name Unicode Conversion */
/*@{*/
/** 
 * An opaque data object used to iterate through Unicode text for 
 * conversion to font encodings.
 *
 * See also the functions that use this object.
 *
 * @see INTL_UnicodeToStrIteratorCreate
 * @see INTL_UnicodeToStrIterate
 * @see INTL_UnicodeToStrIteratorDestroy
 *
*/
typedef void* INTL_UnicodeToStrIterator ;

/**
 * Create an INTL_UnicodeToStrIterator and iterate through it once.
 *
 * This function creates an INTL_UnicodeToStrIterator and iterates through it
 * once to get the first element of Unicode text for font encoding conversion.
 * The function uses the prioritized Character Set ID list (CSIDList) to
 * decide which font encoding it will convert to. The iteration stops if the
 * whole Unicode string is converted. Otherwise, it continues iterating and
 * uses the next charset in the CSIDlist to convert the Unicode text.
 *
 * @param    ustr		Specifies Unicode string to be converted
 * @param    ustrlen	Specifies length of ustr in UCS-2 units not bytes
 * @param    encoding	Returns the encoding of the first element. 
 *						Returns 0 if there are no more to iterate.
 * @param    dest		Specifies the buffer for output and returns the
 *						converted string for the first iteration
 * @param    destbuflen	Specifies the length of dest in bytes
 * @return   			Iterator which keeps the iteration state
 * @see      INTL_GetUnicodeCSIDList
 * @see      INTL_SetUnicodeCSIDList
 * @see      INTL_UnicodeToStrIterate
 * @see      INTL_UnicodeToStrIteratorDestroy
 * @see      INTL_GetUnicodeCharsetList
 */
PUBLIC INTL_UnicodeToStrIterator INTL_UnicodeToStrIteratorCreate(
    INTL_Unicode* ustr,
    uint32 ustrlen,
    INTL_Encoding_ID *encoding,
    unsigned char* dest, 
    uint32 destbuflen
);

/**
 * Iterate through a Unicode object and convert to font encoding.
 * 
 * Iterate the INTL_UnicodeToStrIterator to get Unicode to font encoding
 * conversion.
 *
 * @param iterator		Specifies iterator that keeps the last iteration state
 * @param encoding		Returns the encoding of the first element. Returns 0
 *						if there are no more to iterate.
 * @param dest			Specifies the buffer for output and returns the
 *						converted string for the current iteration
 * @param destbuflen	Specifies the length of dest in bytes
 * @return				0 if there are no more elements to iterate.
 * @see INTL_GetUnicodeCSIDList
 * @see INTL_SetUnicodeCSIDList
 * @see INTL_UnicodeToStrIteratorCreate
 * @see INTL_UnicodeToStrIteratorDestroy
 * @see INTL_GetUnicodeCharsetList
 */
PUBLIC int INTL_UnicodeToStrIterate(
    INTL_UnicodeToStrIterator iterator,
    INTL_Encoding_ID *encoding,
    unsigned char* dest, 
    uint32 destbuflen
);

/**
 * Destroy an INTL_UnicodeToStrIterator.
 *
 * This function destroys the INTL_UnicodeToStrIterator created by 
 * INTL_UnicodeToStrIterateCreate.
 *
 * @param    iterator    Specifies the iterator to be destroyed
 * @see      INTL_GetUnicodeCSIDList
 * @see      INTL_SetUnicodeCSIDList
 * @see      INTL_UnicodeToStrIteratorCreate
 * @see      INTL_UnicodeToStrIterate
 * @see      INTL_GetUnicodeCharsetList
 */
PUBLIC void INTL_UnicodeToStrIteratorDestroy(
	INTL_UnicodeToStrIterator iterator
);

/**
 * Return memory requirement for INTL_UnicodeToStr.
 *
 * Returns the maximum memory required for text converted from a Unicode
 * string to a specified encoding. Call this to prepare memory for
 * INTL_UnicodeToStr.
 *
 * @param    encoding	Specifies the target encoding
 * @param    ustr		Specifies the buffer containing UCS-2 data
 * @param    ustrlen	Specifies the valid length of ustr in UCS-2 units
 *						not bytes
 * @return				Number of bytes needed to store the converted result
 * @see      INTL_UnicodeToStr
 */
PUBLIC uint32 INTL_UnicodeToStrLen(
    INTL_Encoding_ID encoding,
    INTL_Unicode* ustr,
    uint32 ustrlen
);

/**
 * Convert Unicode string to a specified encoding.
 *
 * The caller needs to call INTL_UnicodeToStrLen first to prepare memory and
 * pass into dest.
 *
 * @param    encoding	Specifies the target encoding
 * @param    ustr    	Specifies the buffer containing UCS-2 data
 * @param    ustrlen	Specifies the valid length of ustr in UCS-2 units
 *                   	not bytes
 * @param    dest		Specifies the buffer for the converted text and 
 *                  	returns the converted text
 * @param    destbuflen	Specifies the size of dest in bytes
 * @see INTL_UnicodeToStrLen
 */
PUBLIC void    INTL_UnicodeToStr(
    INTL_Encoding_ID encoding,
    INTL_Unicode* ustr, 
    uint32 ustrlen,
    unsigned char* dest, 
    uint32 destbuflen
);

/**
 * Convert Unicode to text in one encoding by trial and error.
 * 
 * This routine tries to convert the given Unicode string into text of one
 * non-Unicode encoding. This is a trial and error function which may be 
 * slow in "THE WORST CASE". However, it does it's best in the best case and
 * average case. 
 *
 * @param    ustr		Specifies the buffer containing UCS-2 data
 * @param    ustrlen	Specifies the valid length of ustr in UCS-2 units
 *                   	not bytes
 * @param    dest		Specifies the buffer for the converted text and 
 *               		returns the converted text
 * @return				Encoding of the converted text
 */
PUBLIC INTL_Encoding_ID    INTL_UnicodeToEncodingStr(
    INTL_Unicode*    ustr,
    uint32  ustrlen,
    unsigned char*   dest,
    uint32           destbuflen
);

/**
 * Return memory requirement for INTL_StrToUnicode.
 *
 * Return the maximum memory requirement for text converted from the 
 * specified encoding to Unicode. Call this to prepare memory for 
 * INTL_StrToUnicode. The difference between INTL_TextToUnicodeLen is
 * the input string is specified by a NULL terminated string. 
 *
 * @param    encoding	Specifies the encoding of text in src
 * @param    src		Specifies the text to be converted
 * @return   			Size of Unicode to store the converted output (in
 *						UCS-2 units not bytes)
 * @see      INTL_StrToUnicode
 * @see      INTL_TextToUnicodeLen
 */
PUBLIC uint32 INTL_StrToUnicodeLen(
    INTL_Encoding_ID encoding,
    unsigned char* src 
);

/**
 * Convert non-Unicode text to Unicode. 
 *
 * The caller needs to call INTL_StrToUnicodeLen first to prepare memory and
 * pass into ustr. The difference between INTL_TextToUnicode is the input
 * string is specified by a NULL terminated string.
 *
 * @param encoding	Specifies the encoding of text in src
 * @param src		Specifies the text to be converted
 * @param ustr		Specifies the buffer for Unicode and returns the converted
 *                  Unicode
 * @param ubuflen	Specifies the size of the ustr in UCS-2 units not bytes
 * @return			Size of the converted Unicode (in UCS-2 units not bytes)
 * @see INTL_StrToUnicodeLen
 * @see INTL_TextToUnicode
 */
PUBLIC uint32    INTL_StrToUnicode(
    INTL_Encoding_ID encoding,
    unsigned char* src, 
    INTL_Unicode* ustr, 
    uint32 ubuflen
);

/**
 * Return memory requirement for INTL_TextToUnicode.
 *
 * Return the maximum memory requirement for text converted from a specified 
 * encoding to Unicode . Call this to prepare memory for INTL_TextToUnicode. 
 * The difference between INTL_StrToUnicodeLen is the input is not specified 
 * by a NULL terminated string, but a pointer and length.
 *
 * @param encoding  Specifies the encoding of text in src
 * @param src       Specifies the text to be converted
 * @param srclen    Specifies the number of bytes in src
 * @return			Size of Unicode to store the converted output (in UCS-2
 *					units not bytes)
 * @see INTL_TextToUnicode
 * @see INTL_StrToUnicodeLen
 */
PUBLIC uint32 INTL_TextToUnicodeLen(
    INTL_Encoding_ID encoding,
    unsigned char* src,
    uint32 srclen
);

/**
 * Convert text from non-Unicode to Unicode. 
 *
 * The caller needs to call INTL_TextToUnicodeLen first to prepare memory and 
 * pass into ustr. The difference between INTL_StrToUnicode is the input is
 * not specified by a NULL terminated string, but a pointer and length.
 *
 * @param encoding  Specifies the encoding of text in src
 * @param src       Specifies the text to be converted
 * @param srclen    Specifies the number of bytes in src
 * @param ustr      Specifies the buffer for the Unicode string and returns
 *                  the converted Unicode string
 * @param ubuflen   Specifies the size of the ustr in the UCS-2 units not
 *                  bytes
 * @return			Size of converted Unicode (in UCS-2 units not bytes)
 * @see INTL_TextToUnicodeLen
 * @see INTL_StrToUnicode
 */
PUBLIC uint32 INTL_TextToUnicode(
    INTL_Encoding_ID encoding,
    unsigned char* src, 
    uint32 srclen,
    INTL_Unicode* ustr, 
    uint32 ubuflen
);


/**
 * Initial Unicode conversion routines from a list of Character Set ID (CSID)
 * for Unicode rendering.
 *
 * It should only be called once in the application life time. It should be
 * called by front end before calling any other Unicode conversion functions.
 * The list could be retrieved through INTL_GetUnicodeCSIDList or
 * INTL_GetUnicodeCharsetList.
 * 
 * @param    numberOfItem    Specifies the valid number in the csidlist
 * @param    csidlist    Specifies a prioritized list of csid to be used for
 *                       Unicode to font charset conversion. The function will
 *                       make a copy of the list the caller pass in. The caller
 *                       could free the pass in list after this function.
 * @ see     INTL_GetUnicodeCSIDList
 * @ see     INTL_UnicodeToStrIteratorCreate
 * @ see     INTL_UnicodeToStrIterate
 * @ see     INTL_UnicodeToStrIteratorDestroy
 * @ see     INTL_GetUnicodeCharsetList
 */
PUBLIC void INTL_SetUnicodeCSIDList(
    uint16 numOfItems, 
    int16 *csidlist);	

/**
 * Returns a list of Character Set ID (CSID) used for converting Unicode
 * to font encoding. 
 * 
 * The list is set in the initialization time by the front end through 
 * INTL_SetUnicodeCSIDList. The only difference between INTL_GetUnicodeCSIDList
 * and INTL_GetUnicodeCharsetList is that INTL_GetUnicodeCSIDList returns a 
 * list of CSIDs and the INTL_GetUnicodeCharsetList returns a list of charset 
 * names (strings).
 *
 * @param    outnum    Returns the number of items in the returned CSID array.
 * @return		Array of CSIDs. Caller should change or free the returned array.
 * @see      INTL_SetUnicodeCSIDList
 * @see      INTL_UnicodeToStrIteratorCreate
 * @see      INTL_UnicodeToStrIterate
 * @see      INTL_UnicodeToStrIteratorDestroy
 * @see      INTL_GetUnicodeCharsetList
 */
PUBLIC int16*  INTL_GetUnicodeCSIDList(int16 * outnum);

/**
 * Return a list of charset names (strings) used for converting Unicode to font
 * encoding.
 *
 * The list is set in the initialization time by front end through 
 * INTL_SetUnicodeCSIDList. The only difference between INTL_GetUnicodeCSIDList
 * and INTL_GetUnicodeCharsetList is that INTL_GetUnicodeCSIDList returns a
 * list of CSIDs and INTL_GetUnicodeCharsetList returns a list of charset
 * names (strings).
 *
 * @param outnum	Returns the number of items in the returned charset array
 * @return		Array of charset names. Caller should not change or free the 
 *				returned array.
 * @see      INTL_GetUnicodeCSIDList
 * @see      INTL_SetUnicodeCSIDList
 * @see      INTL_UnicodeToStrIteratorCreate
 * @see      INTL_UnicodeToStrIterate
 * @see      INTL_UnicodeToStrIteratorDestroy
 */
PUBLIC unsigned char **INTL_GetUnicodeCharsetList(int16 * outnum);

/**
 * Converts a UTF-8 sub-string to the appropriate font encoding.
 *
 * Converts characters until the encoding changes or
 * input/output space runs out.
 *
 * The segment is NOT NULL TERMINATED
 *
 * @param    utf8p			Specifies the UTF-8 string
 * @param    utf8len		Specifies the length of utf8p 
 * @param    LE_string		Specifies and returns the (pre-allocated) buffer
 *							for the string converted to the font encoding
 * @param    LE_string_len	Specifies the length of the buffer for LE_string
 * @param    LE_written_len	Returns the valid length of the return LE_string
 * @param    LE_string_csid	Returns the CSID of the return LE_string:
 * <UL>
 * <LI>
 *								>0 if successful (valid CSID).
 * <LI>
 *								-1 if not Unicode.
 * <LI>
 *								-2 if no font encoding.
 * </UL>
 * @return					Length of converted UTF-8 string
 */
PUBLIC int utf8_to_local_encoding(
    const unsigned char *utf8p, 
    const int utf8len,
    unsigned char *LE_string, 
    int LE_string_len,
    int *LE_written_len, 
    int16 *LE_string_csid
);

/**
 * Convert text from UTF-8 to UCS-2 encoding.
 *
 * UCS-2 is the abbreviation for the two byte form of Unicode.
 * UTF-8 is a transformation encoding for Unicode.
 * For more information about UTF-8 look at RFC 2279 in
 * <A HREF=ftp://ds.internic.net/rfc/rfc2279.txt>
 *         ftp://ds.internic.net/rfc/rfc2279.txt</A> .
 * For more information about UCS-2, look at <A HREF=http://www.unicode.org>
 * http://www.unicode.org</A>.
 *
 * @param utf8p		Specifies the UTF-8 text buffer. It is NULL terminated.
 * @param num_chars	Returns the length of the converted UCS-2 in UCS-2 units
 *					not bytes
 * @return			UCS-2 string, NULL terminated by U+0000, or NULL. The
 *					caller should free it by calling XP_FREE when it is no
 *					longer needed.
 * @see INTL_UCS2ToUTF8
 */
PUBLIC UNICVTAPI uint16 *INTL_UTF8ToUCS2(
    const unsigned char *utf8p, 
    int32 *num_chars
);

/**
 * Convert text from UCS-2 to UTF-8 encoding.
 *
 * UCS-2 is the abbreviation for the two byte form of Unicode.
 * UTF-8 is a transformation encoding for Unicode.
 * For more information about UTF-8 look at RFC 2279 in
 * <A HREF=ftp://ds.internic.net/rfc/rfc2279.txt>
 * ftp://ds.internic.net/rfc/rfc2279.txt</A> .
 * For more information about UCS-2, look at <A HREF= http://www.unicode.org>
 * http://www.unicode.org</A>.
 *
 * @param ucs2p		Specifies the UCS-2 text buffer
 * @param num_chars	Specifies the length of ucs2p, in UCS-2 units not bytes
 * @return			NULL terminated UTF-8 string or NULL. The caller should
 *					free it by calling XP_FREE when it is no longer needed.
 * @see INTL_UTF8ToUCS2
 */
PUBLIC UNICVTAPI unsigned char *INTL_UCS2ToUTF8(
    const uint16 *ucs2p, 
    int32 num_chars
); 
/*@}*/
/*=======================================================*/
/**@name String Comparison */
/*@{*/

/**
 * Case insensitive comparison. 
 *
 * This function is multibyte charset safe. It will consider characters 
 * boundary correctly. It also ignore case by considering the charset 
 * it used.
 *
 * @param    charSetID    Specifies the encoding of text1 and text2.
 * @param text1    Specifies address of text1.
 * @param text2    Specifies address of text2.
 * @param    charlen    Returns the length in byte of text1.
 * @return   true if the text1 and text2 point to the same character, 
 *           ignoring the case, false otherwise.
 * @see      INTL_MatchOneCaseChar 
 * @see      INTL_Strstr 
 * @see      INTL_Strcasestr 
 */
PUBLIC XP_Bool INTL_MatchOneChar(
    int16 charSetID, 
    unsigned char *text1,
    unsigned char *text2,
    int *charlen
);

/**
 * Case sensitive comparison. 
 *
 * This function is multibyte charset safe. It will consider characters 
 * boundary correctly.
 *
 * @param charSetID     Specifies the encoding of text1 and text2.
 * @param text1    Specifies address of text1.
 * @param text2    Specifies address of text2.
 * @param charlen  Returns length in bytes of text1.
 * @return true if the text1 and text2 point to the same character (same case), 
 *         false otherwise.
 * @see INTL_MatchOneChar 
 * @see INTL_Strstr 
 * @see INTL_Strcasestr 
 */
PUBLIC XP_Bool INTL_MatchOneCaseChar(
    int16 charSetID, 
    unsigned char *text1,
    unsigned char *text2,
    int *charlen
);

/**
 * Case sensitive sub-string search. 
 *
 * This function is multibyte charset safe. It will consider characters 
 * boundary correctly. 
 *
 * @param    charSetID    Specifies the encoding of s1 and s2.
 * @param    s1    Specifies the first string
 * @param    s2    Specifies the second string
 * @return   NULL if s1 does not contains s2, 
 *           otherwise, return the address of the sub-string in s1.
 * @see      INTL_MatchOneChar 
 * @see      INTL_MatchOneCaseChar 
 * @see      INTL_Strcasestr 
 */
PUBLIC char *INTL_Strstr(
    int16 charSetID, 
    const char *s1,
    const char *s2
);

/**
 * Case insensitive sub-string search. 
 *
 * This function is multibyte charset safe. It will consider characters 
 * boundary correctly. It also ignore case by considering the charset it 
 * used.
 *
 * @param    charSetID    Specifies the encoding of s1 and s2.
 * @param    s1    Specifies the first string
 * @param    s2    Specifies the second string
 * @return   NULL if s1 does not contains s2, 
 *           otherwise, return the address of the sub-string in s1.
 * @see      INTL_MatchOneChar 
 * @see      INTL_MatchOneCaseChar 
 * @see      INTL_Strstr 
 */
PUBLIC char *INTL_Strcasestr(
    int16 charSetID, 
    const char *s1, 
    const char *s2
);


/*
  Function to support correct mail/news comparison:
	INTL_GetNormalizeStr
	INTL_GetNormalizeStrFromRFC1522
	INTL_StrContains
	INTL_StrIs
	INTL_StrBeginWith
	INTL_StrEndWith

  Example:

	XP_Bool MailHeaderContains(csid, header, str)
	{
		XP_Bool result = FALSE;
		unsigned char* n_str = INTL_GetNormalizeStr(csid, str);
		unsigned char* n_header = INTL_GetNormalizeStrFromRFC1522(csid, header);

		if((NULL != n_str) && (NULL != n_header))
			result = INTL_StrContains(csid, n_header, n_str);
		if(n_str)
			XP_FREE(n_str);
		if(n_header)
			XP_FREE(n_header);
		return result;
	}

*/

/**
 * Normalize a string, by dropping the case of the characters.
 *
 * The return value could be used with INTL_StrContains, INTL_StrIs,
 * INTL_StrBeginWith or INTL_StrEndWith to perform string matching. This
 * function normalizes a string by dropping the case of character according to
 * the charSetID the caller passed in. It also ignores CR and LF characters.
 *
 * @param    charSetID    Specifies the encoding of str
 * @param    str    Specifies the to-be-normalized string.
 * @return a normalized string which could be used in  INTL_StrContains, 
 *         INTL_StrIs, INTL_StrBeginWith and INTL_StrEndWith The caller should
 *         free it by calling XP_FREE when it is not needed.
 * @see      INTL_GetNormalizeStrFromRFC1522
 * @see      INTL_StrContains
 * @see      INTL_StrIs
 * @see      INTL_StrBeginWith
 * @see      INTL_StrEndWith
 */
PUBLIC unsigned char* INTL_GetNormalizeStr(
    int16 charSetID, 
    unsigned char* str
);

/**
 * Test if string s1 contains string s2.
 *
 * This function is multibyte charset safe. It will consider characters
 * boundary correctly.  To do string matching with ignoring the case of
 * character, call INTL_GetNormalizeStr (or INTL_GetNormalizeStrFromRFC1522)
 * before call this function.
 *
 * @param    charSetID    Specifies the encoding for s1 and s2.
 * @param    s1    Specifies the first string
 * @param    s2    Specifies the second string
 * @return   true if s1 contains s2, 
 *           false otherwise
 * @see      INTL_GetNormalizeStr
 * @see      INTL_GetNormalizeStrFromRFC1522
 * @see      INTL_StrIs
 * @see      INTL_StrBeginWith
 * @see      INTL_StrEndWith
 */
PUBLIC XP_Bool INTL_StrContains(
    int16 charSetID, 
    unsigned char* str1, 
    unsigned char* str2
);

/**
 * Test if string s1 is string s2.
 *
 * This function is multibyte charset safe. It will consider characters boundary
 * correctly.  To do string matching with ignoring the case of character, call
 * INTL_GetNormalizeStr (or INTL_GetNormalizeStrFromRFC1522) before calling this
 * function.
 *
 * @param    charSetID    Specifies the encoding for s1 and s2.
 * @param    s1    Specifies the first string
 * @param    s2    Specifies the second string
 * @return   true if two string are equal, false otherwise
 * @see      INTL_GetNormalizeStr
 * @see      INTL_GetNormalizeStrFromRFC1522
 * @see      INTL_StrContains
 * @see      INTL_StrBeginWith
 * @see      INTL_StrEndWith
 */
PUBLIC XP_Bool INTL_StrIs(
    int16 charSetID, 
    unsigned char* str1, 
    unsigned char* str2
);

/**
 * Test if string s1 begin with string s2.
 *
 * This function is multibyte charset safe. It will consider characters
 * boundary correctly.  To do string matching with ignoring the case of 
 * character, call INTL_GetNormalizeStr (or INTL_GetNormalizeStrFromRFC1522)
 * before calling this function.
 *
 * @param    charSetID    Specifies the encoding for s1 and s2.
 * @param    s1    Specifies the first string
 * @param    s2    Specifies the second string
 * @return   true if the first string is begin with the second string, 
 *           false otherwise
 * @see      INTL_GetNormalizeStr
 * @see      INTL_GetNormalizeStrFromRFC1522
 * @see      INTL_StrContains
 * @see      INTL_StrIs
 * @see      INTL_StrEndWith
 */
PUBLIC XP_Bool INTL_StrBeginWith(
    int16 charSetID, 
    unsigned char* str1, 
    unsigned char* str2
);

/**
 * Test if string s1 end with string s2.
 *
 * This function is multibyte charset safe. It will consider characters 
 * boundary correctly. To do string matching with ignoring the case of 
 * character, call INTL_GetNormalizeStr (or INTL_GetNormalizeStrFromRFC1522) 
 * before calling this function.
 *
 * @param    charSetID    Specifies the encoding for s1 and s2.
 * @param    s1    Specifies the first string
 * @param    s2    Specifies the second string
 * @return true if the first string is end with the second string, false
 *         otherwise.
 * @see      INTL_GetNormalizeStr
 * @see      INTL_GetNormalizeStrFromRFC1522
 * @see      INTL_StrContains
 * @see      INTL_StrIs
 * @see      INTL_StrBeginWith
 */
PUBLIC XP_Bool INTL_StrEndWith(
    int16 charSetID, 
    unsigned char* str1, 
    unsigned char* str2
);

/** 
 * Decode, convert and create a message header. Then create and return a collatable string. 
 * 
 * If the message header contains an RFC 2047 encoded-word, that word is 
 * decoded. Then it performs charset conversion to window charset ID. 
 * Finally, it creates and returns a machine collatable string (calls INTL_CreateCollationKeyByDefaultLocale)  
 * which can be compared by INTL_Compare_CollationKey. 
 * 
 * @param header          Specifies the message string to be decoded/converted. 
 * @param wincsid         Specifies the target window charset ID. 
 * @param collation_flag  For future enhancement, pass 0 for now. 
 * @return A null terminated string collatable by INTL_Compare_CollationKey. 
 *         The caller must free the output buffer by calling XP_FREE when it is no longer needed. 
 * @see INTL_DECODE_MIME_PART_II,INTL_Compare_CollationKey,INTL_CreateCollationKeyByDefaultLocale 
 */ 
char *INTL_DecodeMimePartIIAndCreateCollationKey(const char *header, int16 wincsid, int32 collation_flag);

/** 
 * Create a collation key using default sytem locale. 
 * 
 * By using default system locale, this creates and returns a machine collatable string 
 * which can be compared by INTL_Compare_CollationKey. 
 * 
 * @param in_string       Input string for a key generation. 
 * @param wincsid         Specifies the target window charset ID. 
 * @param collation_flag  For future enhancement, pass 0 for now. 
 * @return A null terminated string collatable by INTL_Compare_CollationKey. 
 *         The caller must free the output buffer by calling XP_FREE when it is no longer needed. 
 * @see INTL_DecodeMimePartIIAndCreateCollationKey,INTL_Compare_CollationKey 
 */ 
char *INTL_CreateCollationKeyByDefaultLocale(const char *in_string, int16 wincsid, int32 collation_flag);

/** 
 * Compare two collation keys. 
 * 
 * Compare two collation keys generated by INTL_CreateCollationKeyByDefaultLocale. 
 * 
 * @param key1            Null terminated string.
 * @param key2            Null terminated string.
 * @return <0 if key1 less than key2
 *         0  if key1 equals to key2
 *         >0 if key1 greater than key2
 * @see INTL_DecodeMimePartIIAndCreateCollationKey,INTL_Compare_CollationKey 
 */ 
int INTL_Compare_CollationKey(const char *key1, const char *key2);

/**
 * Return a (hacky) XPAT pattern for NNTP server for searching pre 
 * RFC 1522 message header.
 *
 * This is a hacky function which try to work around another HACK!!! The 
 * problem it tries to solve is to search on NNTP, internet newsgroup server.   
 * Unfortunately, the NNTP server does not have non-ASCII text searching
 * command. The only functionality in the NNTP protocol we could use is the
 * XPAT extension of NNTP (see
 * <A HREF=ftp://ds.internic.net/internet-drafts/draft-ietf-nntpext-imp-01.txt>
 * ftp://ds.internic.net/internet-drafts/draft-ietf-nntpext-imp-01.txt</A> or
 * <A HREF=ftp://ds.internic.net/internet-drafts/draft-barber-nntp-imp-07.txt>
 * ftp://ds.internic.net/internet-drafts/draft-barber-nntp-imp-07.txt</A> ).
 * XPAT use wildmat regular expression (see <A HREF=
 * http://oac.hsc.uth.tmc.edu/oac_sysadmin/services/INN/man/wildmat.3.html>
 * http://oac.hsc.uth.tmc.edu/oac_sysadmin/services/INN/man/wildmat.3.html</A>
 * for details) to provide string matching. Unfortunately, wildmat is not
 * designed to support non-ASCII text. It work for English header but not for
 * header in other language like Japanese, French, or German. The problem is
 * the XPAT/wildmat cannot deal with (1) ISO-2022-xx encoding nor (2) RFC 1522
 * header. To work around the limitation in the protocol, we put together this
 * function to support the first limitation as possible as we can. This
 * function take one search string, and return a XPAT pattern which could then
 * be used to send to NNTP XPAT as search argument. However, there are some
 * limitation here. (1) It may cause NNTP return more message than it should,
 * the reason is the XPAT won't respect to the multibyte character boundary
 * when it try to match the string. To improve this in the future, the client
 * double check the header after it receive message from the server and narrow
 * it down to the correct case.  (2) The pattern it generated won't match RFC
 * 1522 header so it could return less message than it should. This is because
 * there are more than one XPAT could match the sting in the case of RFC 1522
 * header. To improve this in the future, the client side should send several
 * possible XPAT patterns (with the patterned return by this function), collect
 * the result, and then double checking in the client side. Of course, improve
 * the NNTP protocol itself is the real solution. But the improvement stated
 * above is also needed for the server support the current NNTP protocol.  This
 * function (1) convert the text from the encoding the argument specified into
 * the encoding used in the corresponding internet newsgroup, (2) strip out
 * leading or trailing ISO-2022 escape sequence if present, (3) escape the
 * wildmat special characters (any characters which is not from 0-9, a-z, A-Z),
 * and return.
 *
 * @param winCharSetID       Specifies the encoding of searchString.
 * @param searchString  Specifies the string to be search through NNTP XPAT
 *                      command.
 * @return the pattern should be send to NNTP XPAT command for searching
 *         non-ASCII header. The caller need to free this by calling XP_FREE
 *         when the result is no longer needed.
 */
PUBLIC unsigned char* INTL_FormatNNTPXPATInNonRFC1522Format(
    int16 winCharSetID, 
    unsigned char* searchString
);

/*@}*/
/*=======================================================*/
/**@name Charset ID Iterator */
/*@{*/


/** 
 * An object that can iterate through a list of charset ID.
 *
 * @see INTL_CSIDIteratorCreate
 * @see INTL_CSIDIteratorDestroy
 * @see INTL_CSIDIteratorNext
 */
typedef void* INTL_CSIDIterator;	

/**
 * Returns a new iterator object to search charset IDs for a particular
 * conversion.
 *
 * This function searches a built-in table to look for charset converters
 * that could be used for a particular purpose. The only purpose currently
 * supported is the IMAP4 conversion. This function puts the mail and news
 * charset IDs corresponding to the given charset ID at the top of the list
 * of IDs to try. After that, it inserts the "to" charset IDs of all entries
 * matching the given "from" ID.
 *
 * @param iterator_return  Returns a new iterator object
 * @param charSetID        Specifies the charset ID to convert from
 * @param flag             Specifies the type of conversion
 *                         Currently, the only valid value is 
 *                         csiditerate_TryIMAP4Search  .
 * 
 * @see INTL_CSIDIteratorNext, INTL_CSIDIteratorDestroy
 */
PUBLIC void INTL_CSIDIteratorCreate(
    INTL_CSIDIterator *iterator,
    int16 charSetID,
    int flag
);

/**
 * Frees the given iterator, and sets given pointer to NULL.
 *
 * This function destroys the object created by INTL_CSIDIteratorCreate.
 *
 * @param iterator  Specifies the iterator object to destroy
 * @see INTL_CSIDIteratorCreate
 */
PUBLIC void INTL_CSIDIteratorDestroy(
    INTL_CSIDIterator *iterator
);

/**
 * Returns the next charset ID in the given iterator, if any.
 *
 * The return value is TRUE if a charset ID was found. The charset ID
 * is returned in pCharSetID. Otherwise, the return value is FALSE, and
 * pCharSetID remains untouched.
 *
 * @param iterator    Specifies the iterator object
 * @param pCharSetID  Returns the next charset ID
 * @return TRUE if there are more elements to be iterate, otherwise FALSE
 * @see INTL_CSIDIteratorCreate, INTL_CSIDIteratorDestroy
 */
PUBLIC XP_Bool INTL_CSIDIteratorNext(
    INTL_CSIDIterator *iterator,
    int16 *pCharSetID
);

/*@}*/
/*=======================================================*/
/**@name Line/Word Breaking */
/*@{*/

/**
 *  Line breaking information.
 * 
 *  <UL>
 *  <LI>
 *  PROHIBIT_NOWHERE - 
 *  It is a breakable character. It could be break before 
 *      or after this character. This class is for all 
 *      Kanji ideographic character.
 *  <LI>
 *  PROHIBIT_BEGIN_OF_LINE - 
 *  It should not appeared in the beginning of the line.
 *  <LI>
 *  PROHIBIT_END_OF_LINE - 
 *  It should not appeared in the end of the line.
 *  <LI>
 *  PROHIBIT_WORD_BREAK - 
 *  It is non breakable character. It cannot be break
 *      if the next (or previous) character is also 
 *      PROHIBIT_WORD_BREAK. 
 *  </UL>
 *
 * @see INTL_KinsokuClass
 */
enum LINE_WRAP_PROHIBIT_CLASS{
    PROHIBIT_NOWHERE,
    PROHIBIT_BEGIN_OF_LINE,
    PROHIBIT_END_OF_LINE,
    PROHIBIT_WORD_BREAK
};

/**
 * Basic Japanese word breaking information.
 * 
 * <UL>
 * <LI>
 * SEVEN_BIT_CHAR - e.g. ASCII
 * <LI>
 * HALFWIDTH_PRONOUNCE_CHAR - e.g. Japanese Katakana
 * <LI>
 * FULLWIDTH_ASCII_CHAR - e.g. ASCII in JIS
 * <LI>
 * FULLWIDTH_PRONOUNCE_CHAR - e.g. Japanese Hiragana, Katakana
 * <LI>
 * KANJI_CHAR - ideographic
 * <LI>
 * UNCLASSIFIED_CHAR - others
 * </UL>
 * 
 * @see       INTL_CharClass
 */
enum WORD_BREAK_CLASS{
    SEVEN_BIT_CHAR,
    HALFWIDTH_PRONOUNCE_CHAR,
    FULLWIDTH_ASCII_CHAR,
    FULLWIDTH_PRONOUNCE_CHAR,
    KANJI_CHAR,
    UNCLASSIFIED_CHAR
};
/**
 * Returns the code point that represent the non-breaking space character.
 * 
 * The current implementation return the same value regardless of the given
 * charset. However, the return value is platform dependent.
 * The information then is used by parser and layout code.
 * 
 * Using this function with caution as it is tied to
 * the current HTML parser implementation.
 * 
 * @param     winCharSetID   Specifies the window charset id.
 * @return    the code point which Non Breaking Space in a 
 *            C style NULL terminated string.
 * @see       
 */
PUBLIC const char *INTL_NonBreakingSpace(
    uint16 winCharSetID
);

/**
 * Returns information for basic Japanese word breaking.
 *
 * Given a character pointer and charset, returns a word breaking 
 * character class for the given character.
 * It is necessary to pass a pointer because the
 * character may be more than one byte.
 *
 * In the future, the definition of word breaking classes needs to be 
 * extended.
 * 
 * @param     winCharSetID	Specifies the window charset ID
 * @param     pstr			Specifies the pointer to the character 
 * @return					Character class for word breaking:
 * <UL>
 * <LI>
 * SEVEN_BIT_CHAR - e.g. ASCII
 * <LI>
 * HALFWIDTH_PRONOUNCE_CHAR - e.g. Japanese Katakana
 * <LI>
 * FULLWIDTH_ASCII_CHAR - e.g. ASCII in JIS
 * <LI>
 * FULLWIDTH_PRONOUNCE_CHAR - e.g. Japanese Hiragana, Katakana
 * <LI>
 * KANJI_CHAR - ideographic
 * <LI>
 * UNCLASSIFIED_CHAR - others
 * </UL>
 * @see       INTL_KinsokuClass
 * @see       WORD_BREAK_CLASS
 */
PUBLIC int INTL_CharClass(
    int winCharSetID, 
    unsigned char *pstr
);

/**
 * Returns line breaking information.
 *
 * Given a character pointer and charset, returns a line breaking 
 * character class for the given character.
 * It is necessary to pass a pointer because the
 * character may be more than one byte.
 *
 *
 * Please notice that the function currently only supports multibyte charsets.
 * If this is called for ascii charset, it always return PROHIBIT_WORD_BREAK.
 * 
 * References for line breaking:
 * <UL>
 * <LI>
 * Japanese Standard Association,
 * JIS X 4501 1995 - Japanese Industrial Standard - 
 *     Line Composition rules for Japanese documents
 * <LI>
 * Ken Lunde,
 * Understanding Japanese Information Processing,
 * O'Reilly &amp; Associates, Inc.,
 * ISBN:1-56592-043-0, 
 * pp.148
 * <LI>
 * Nadine Kano,
 * Developing International Software For Windows 95 and Windows NT,
 * Microsoft Press,
 * ISBN:1-556-15-840-8,
 * pp.239-244
 * </UL>
 * 
 * 
 * @param     winCharSetID   Specifies window charset ID.
 * @param     pstr    Specifies the pointer to the character 
 * @return    the kinsoku class for line breaking:
 * <UL>
 * <LI>
 *  PROHIBIT_NOWHERE - 
 *  It is a breakable character. It could be break before 
 *      or after this character. This class is for all 
 *      Kanji ideographic character.
 * <LI>
 *  PROHIBIT_BEGIN_OF_LINE - 
 *  It should not appeared in the beginning of the line.
 * <LI>
 *  PROHIBIT_END_OF_LINE - 
 *  It should not appeared in the end of the line.
 * <LI>
 *  PROHIBIT_WORD_BREAK - 
 *  It is non breakable character. It cannot be break
 *      if the next (or previous) character is also 
 *      PROHIBIT_WORD_BREAK. 
 * </UL>
 * @see       INTL_CharClass
 * @see       LINE_WRAP_PROHIBIT_CLASS
 */
PUBLIC int INTL_KinsokuClass(
    int16 winCharSetID, 
    unsigned char *pstr
);

/**
 * Returns the column width of the given character.
 *
 * In some countries, old terminals use full-width and half-width characters.
 * This function returns the number of "columns" taken up by the given
 * character. For example, in Japan, normal characters take up 2 columns,
 * while half-width characters take up 1 column each.
 *
 * Returns 1 for charsets that do not distinguish between half-width and
 * full-width characters.
 *
 * @param winCharSetID  Specifies the charset ID of the text
 * @param pstr          Specifies the character
 * @return              The column width of the given character
 * @see                 INTL_IsHalfWidth
 */
PUBLIC int INTL_ColumnWidth(
    int winCharSetID,
    unsigned char *pstr
);

/**
 * Truncates a long string by replacing excess characters in the middle
 * with "&#46;&#46;&#46;".
 *
 * The output_return pointer may be the same as the input pointer.
 *
 * @param winCharSetID   Specifies the charset ID of the text
 * @param input          Specifies the text to be mid-truncated
 * @param output_return  Returns the mid-truncated text
 * @param max_length     Specifies the desired number of bytes to be placed in
 *                       the output buffer, minus 1 for null terminator
 */
PUBLIC void INTL_MidTruncateString(
    int16 winCharSetID,
    const char *input,
    char *output_return,
    int max_length
);

/**
 * Returns whether or not the given character is a half-width character.
 *
 * In some countries, certain characters are normal width on old terminals,
 * while other characters are half-width. For example, normal Japanese
 * characters are considered normal width, while "hankaku kana" are
 * half-width, as are the ASCII characters.
 *
 * @param winCharSetID  Specifies the charset ID of the text
 * @param pstr          Specifies the character
 * @return 
 *         0 if the given character is ASCII or the charset do not normally
 *           distinguish between half-width and full-width,
 *         1 if the given character is half-width
 * @see INTL_ColumnWidth
 */
PUBLIC int INTL_IsHalfWidth(
    uint16  winCharSetID,
    unsigned char *pstr
);

/*@}*/
/*=======================================================*/
/**@name Document Context Handling */
/*@{*/
/** 
 * Request a re-layout of the document.
 *
 * Libi18n calls this function in those cases where a different document
 * encoding is detected after document conversion and layout has begun.
 * This can occur because the parsing and layout of the document begins
 * immediately when the document data begins to stream in - at which time
 * all the data needed to determine the charset may not be available.  If
 * this occurs, the layout engine needs to be notified to pull the data from
 * the source (cache) again so the data will be converted by the correct
 * character codeset conversion module in the data stream.
 * 
 * @param context Specifies the context which should be relayout again. 
 */
PUBLIC void 
INTL_Relayout(iDocumentContext context);

/**
 * Returns name of the document charset.
 *
 * The returned string is suitable for use in the window brought up by
 * View | Page Info (previously known as Document Info). It also provides
 * information such as whether this charset was auto-detected.
 *
 * @param  doc_context	Specifies the document context
 * @return				Name (string) of the document charset
 */
PUBLIC char *INTL_CharSetDocInfo(
    iDocumentContext doc_context
);

/**
 * Get the UI charset encoding setting.
 *
 * Gets the currently selected charset encoding for this document 
 * (not the global default and not the detected document encoding).
 * 
 * @param     context    Specifies document context
 * @return    Document charset ID selected by the user
 * @see       
 */
PUBLIC uint16 FE_DefaultDocCharSetID(
    iDocumentContext context
);

/**
 * Change the default document charset ID.
 * 
 * This function is currently only implemented and called by the Windows
 * platform.  It will be removed in the future to keep the consistency between
 * platforms.
 *
 * @param defaultDocCharSetID Specifies the new default document charset ID
 * @version DEPRECATED. Do not use this function.
 */
#if defined(XP_WIN) || defined(XP_OS2)
PUBLIC void
INTL_ChangeDefaultCharSetID(int16 defaultDocCharSetID);
#endif

/**
 * Return default charset from preference or from current encoding 
 * menu selection. 
 * 
 * @param context	Specifies the context
 * @return			Default document charset ID.  If the context is NULL
 *					then it returns default charset from the user preference.
 *					If the context is specified then it returns current
 *					encoding menu selection.
 */
PUBLIC int16
INTL_DefaultDocCharSetID(iDocumentContext context);

/**
 * Returns the default window charset ID for the given document context.
 *
 * If context is NULL, or the context's window charset ID is zero, this
 * function calls INTL_DefaultWinCharSetID, passing the same context.
 *
 * @param context	Specifies the document context
 * @return			The default window charset ID for this document context
 * @see INTL_DefaultWinCharSetID
 */
PUBLIC int16 INTL_DefaultTextAttributeCharSetID(
    iDocumentContext context
);

/**
 * Returns the default window charset ID for the given document context.
 *
 * If context is NULL, or if the context's window charset ID is zero, this
 * function calls INTL_DefaultDocCharSetID, passing the same context, and then
 * calls INTL_DocToWinCharSetID on the result.
 *
 * @param context	Specifies the document context
 * @return			Default window charset ID for this document context
 * @see INTL_DefaultDocCharSetID, INTL_DocToWinCharSetID
 */
PUBLIC int16 INTL_DefaultWinCharSetID(
    iDocumentContext context
);
/**
 * Set up the charset conversion stream module.
 *
 * This function gets the charset info object from the context, and then
 * picks up the relayout flag and the document charset ID before calling
 * INTL_CSIInitialize. It then creates the appropriate charset converter
 * to convert from the document to window charset. The stream is set up
 * by setting the various function pointers (put, abort, complete, etc).
 * It then hooks up to the next stream module "INTERNAL_PARSER", the HTML
 * parser and layout engine. This is done by rewriting URL_s' content_type
 * field.
 *
 * @param format_out  Specifies the type of stream
 * @param data_obj    Ignored
 * @param URL_s       Specifies the URL object
 * @param window_id   Specifies the context
 * @return		Stream object corresponding to this charset conversion module
 * @see INTL_CSIInitialize, NET_StreamBuilder
 */
PUBLIC Stream *INTL_ConvCharCode(
    int format_out,
    void *data_obj,
    URL *URL_s,
    iDocumentContext window_id
);

/**
 * Converts mail charset to display charset used by current window. 
 *
 * It decides which display charset to use based on current default language.
 * Caller is responsible for deallocating memory.
 * 
 * @param context     the context (window ID).
 * @param bit7buff    Source buffer.
 * @param block_size  the length of the source buffer.
 * @return Destination buffer. If NULL, this means either conversion failed or
 *         did single-byte to single-byte conversion.
 */
PUBLIC unsigned char *INTL_ConvMailToWinCharCode(
    iDocumentContext context,
    unsigned char *bit7buff,
    uint32 block_size
);

/*@}*/
/*=======================================================*/
/**@name Platform Independent String Resources */
/*@{*/
/**
 * Return the Charset name of the translated resource.
 *
 * @return	MIME charset of the cross-platform string resource and FE
 * resources
 * @see XP_GetString
 * @see XP_GetStringForHTML
 */
PUBLIC char *
INTL_ResourceCharSet(void);

/*@}*/
/*=======================================================*/


/* Definition for the charset ID selector. 
 */ 
typedef enum { 
    INTL_FileNameCsidSel = 1, 	        /* The file name */
    INTL_DefaultTextWidgetCsidSel,	/* The edit control or text widget */
    INTL_OldBookmarkCsidSel,		/* The bookmark.html file */
    INTL_XPResourcesCsidSel,		/* The cross-platform resources */
    INTL_MenuCsidSel 			/* The menu and menu bar */
} INTL_CharSetID_Selector; 

/* Typedef for charset ID. 
 */ 
typedef int16 INTLCharSetID; 

/** 
 * Get charset ID using a given selector. 
 * 
 * Using a given selector, this returns a charset ID. 
 * Designed to retrieve a non-context dependent charset ID (e.g file system). 
 * 
 * @param     selector    Specification for a charset ID to get. 
 * @return Charset ID for the input selector. Returns CS_DEFUALT in case of error (e.g. selector invalid). 
 */ 

INTLCharSetID INTL_GetCharSetID(INTL_CharSetID_Selector selector); 


const char* INTL_CharsetCorrection(const char* charsetname);

/*
 * Date/Time Format Selectors
 */
typedef PRUint32	INTLLocaleID;
typedef enum INTL_DateFormatSelector {
	INTL_DateFormatNone,
	INTL_DateFormatLong,
	INTL_DateFormatShort,
	INTL_DateFormatYearMonth
} INTL_DateFormatSelector;

typedef enum INTL_TimeFormatSelector {
	INTL_TimeFormatNone,
	INTL_TimeFormatSeconds,
	INTL_TimeFormatNoSeconds,
	INTL_TimeFormatForce24Hour
} INTL_TimeFormatSelector;
/** 
 * Locale sensitive Date/Time formmating function 
 * 
 * Using a given selector, this returns a charset ID. 
 * Designed to retrieve a non-context dependent charset ID (e.g file system). 
 * 
 * @param	localeID		Specification for the Locale conventions to use.
 * @param	formatSelector	Specification for the type of format to select
 * @param	timeToFormat	time
 * @param	utf8Buffer		result buffer (in utf8)
 * @param	bufferLength	length of result buffer
 * @return	PRSuccess when succesful, PRFailure otherwise 
 */ 
PUBLIC PRStatus	INTL_FormatTime(INTLLocaleID localeID,
									INTL_DateFormatSelector dateFormatSelector,
									INTL_TimeFormatSelector	timeFormatSelector,
									time_t	timeToFormat,
									unsigned char* utf8Buffer,
									PRUint32 bufferLength);
/** 
 * Locale sensitive Date formmating function 
 * 
 * Using a given selector, this returns a charset ID. 
 * Designed to retrieve a non-context dependent charset ID (e.g file system). 
 * 
 * @param	localeID		Specification for the Locale conventions to use.
 * @param	formatSelector	Specification for the type of format to select
 * @param	timeToFormat	time
 * @param	utf8Buffer		result buffer (in utf8)
 * @param	bufferLength	length of result buffer
 * @return	PRSuccess when succesful, PRFailure otherwise 
 */ 
PUBLIC PRStatus INTL_FormatTMTime(INTLLocaleID localeID,
									  INTL_DateFormatSelector dateFormatSelector,
									  INTL_TimeFormatSelector	timeFormatSelector,
									  const struct tm* timeToFormat,
									  unsigned char* utf8Buffer,
									  PRUint32 bufferLength);

/*
 * Definition for language country selector. 
 */ 
typedef enum { 
    INTL_LanguageSel = 1, 	        /* User's default language */
    INTL_LanguageCollateSel,		/* User's default language for collation */
    INTL_LanguageMonetarySel,		/* User's default language for monetary format */
    INTL_LanguageNumericSel,		/* User's default language for numeric format */
    INTL_LanguageTimeSel,			/* User's default language for data/time format */
    INTL_CountrySel,				/* User's default country */
    INTL_CountryCollateSel,			/* User's default country for collation */
    INTL_CountryMonetarySel,		/* User's default country for monetary format */
    INTL_CountryNumericSel,			/* User's default country for numeric format */
    INTL_CountryTimeSel, 			/* User's default country for data/time format */
	INTL_ALL_LocalesSel				/* All locales installed to the user's machine */
} INTL_LanguageCountry_Selector; 


/** 
 * Returns an ISO language or country code for a given selector. 
 * 
 * Using a given selector, this returns a language (ISO639) code or country (ISO3166 code2) code in a C string. 
 * 
 * @param     selector    Specification for a name to get. 
 * @return    A string of ISO language or country code specified by the input selector. 
 *            Returns an empty string when the requested information is not available from the system. 
 *            The caller is responsible for deallocalte the memory returned.
 */ 

PUBLIC char *INTL_GetLanguageCountry(INTL_LanguageCountry_Selector selector);

/** 
 * Front end implementation of INTL_GetLanguageCountry. 
 * 
 * Using a given selector, this returns a language (ISO639) code or country (ISO3166 code2) code in a C string
 * 
 * @param     selector    Specification for a name to get. 
 * @return    A string of ISO language or country code specified by the input selector. 
 *            Returns an empty string when the requested information is not available from the system. 
 *            The caller is responsible for deallocalte the memory returned.
 * @see INTL_GetLanguageCountry.
 */ 

PUBLIC char *FE_GetLanguageCountry(INTL_LanguageCountry_Selector selector);


/** 
 * Convert platform specific id to ISO code. 
 * 
 * This function is supposed to be called by the front end code.
 * It converts a platform specific id to a language (ISO639) and a country (ISO3166 code2) code. 
 * 
 * @param     platformIdNum Platform specific id in number. Langid for Windows, language or region code for Macintosh or zero for Unix.
 * @param     platformIdStr Platform specific id in string or NULL for Windows and Macintosh. 
 * @param     bLanguage     TRUE for language, FALSE for country. 
 * @return    A C string of language/country code connected by a hyphen (e.g. "en-US"). 
 *            Returns an empty string when the requested information is not available from the system. 
 *            The caller is should not modify or free the memory returned.
 */ 

PUBLIC const char *INTL_PlatformIdToISOCode(unsigned short platformIdNum, char *platformIdStr, XP_Bool bLanguage); 


XP_END_PROTOS

#endif /* INTL_LIBI18N_H */
