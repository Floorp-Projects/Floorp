/*
The contents of this file are subject to the Mozilla Public License
Version 1.0 (the "License"); you may not use this file except in
compliance with the License. You may obtain a copy of the License at
http://www.mozilla.org/MPL/

Software distributed under the License is distributed on an "AS IS"
basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
License for the specific language governing rights and limitations
under the License.

The Original Code is expat.

The Initial Developer of the Original Code is James Clark.
Portions created by James Clark are Copyright (C) 1998
James Clark. All Rights Reserved.

Contributor(s):
*/

#ifndef XmlParse_INCLUDED
#define XmlParse_INCLUDED 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef XMLPARSEAPI
#define XMLPARSEAPI /* as nothing */
#endif

typedef void *XML_Parser;

#ifdef XML_UNICODE_WCHAR_T

/* XML_UNICODE_WCHAR_T will work only if sizeof(wchar_t) == 2 and wchar_t
uses Unicode. */
/* Information is UTF-16 encoded as wchar_ts */

#ifndef XML_UNICODE
#define XML_UNICODE
#endif

#include <stddef.h>
typedef wchar_t XML_Char;
typedef wchar_t XML_LChar;

#else /* not XML_UNICODE_WCHAR_T */

#ifdef XML_UNICODE

/* Information is UTF-16 encoded as unsigned shorts */
typedef unsigned short XML_Char;
typedef char XML_LChar;

#else /* not XML_UNICODE */

/* Information is UTF-8 encoded. */
typedef char XML_Char;
typedef char XML_LChar;

#endif /* not XML_UNICODE */

#endif /* not XML_UNICODE_WCHAR_T */


/* Constructs a new parser; encoding is the encoding specified by the external
protocol or null if there is none specified. */

XML_Parser XMLPARSEAPI
XML_ParserCreate(const XML_Char *encoding);


/* atts is array of name/value pairs, terminated by 0;
   names and values are 0 terminated. */

typedef void (*XML_StartElementHandler)(void *userData,
					const XML_Char *name,
					const XML_Char **atts);

typedef void (*XML_EndElementHandler)(void *userData,
				      const XML_Char *name);

/* s is not 0 terminated. */
typedef void (*XML_CharacterDataHandler)(void *userData,
					 const XML_Char *s,
					 int len);

/* target and data are 0 terminated */
typedef void (*XML_ProcessingInstructionHandler)(void *userData,
						 const XML_Char *target,
						 const XML_Char *data);

/* This is called for any characters in the XML document for
which there is no applicable handler.  This includes both
characters that are part of markup which is of a kind that is
not reported (comments, markup declarations), or characters
that are part of a construct which could be reported but
for which no handler has been supplied. The characters are passed
exactly as they were in the XML document except that
they will be encoded in UTF-8.  Line boundaries are not normalized.
Note that a byte order mark character is not passed to the default handler.
If a default handler is set, internal entity references
are not expanded. There are no guarantees about
how characters are divided between calls to the default handler:
for example, a comment might be split between multiple calls. */

typedef void (*XML_DefaultHandler)(void *userData,
				   const XML_Char *s,
				   int len);

/* This is called for a declaration of an unparsed (NDATA)
entity.  The base argument is whatever was set by XML_SetBase.
The entityName, systemId and notationName arguments will never be null.
The other arguments may be. */

typedef void (*XML_UnparsedEntityDeclHandler)(void *userData,
					      const XML_Char *entityName,
					      const XML_Char *base,
					      const XML_Char *systemId,
					      const XML_Char *publicId,
					      const XML_Char *notationName);

/* This is called for a declaration of notation.
The base argument is whatever was set by XML_SetBase.
The notationName will never be null.  The other arguments can be. */

typedef void (*XML_NotationDeclHandler)(void *userData,
					const XML_Char *notationName,
					const XML_Char *base,
					const XML_Char *systemId,
					const XML_Char *publicId);

/* This is called for a reference to an external parsed general entity.
The referenced entity is not automatically parsed.
The application can parse it immediately or later using
XML_ExternalEntityParserCreate.
The parser argument is the parser parsing the entity containing the reference;
it can be passed as the parser argument to XML_ExternalEntityParserCreate.
The systemId argument is the system identifier as specified in the entity declaration;
it will not be null.
The base argument is the system identifier that should be used as the base for
resolving systemId if systemId was relative; this is set by XML_SetBase;
it may be null.
The publicId argument is the public identifier as specified in the entity declaration,
or null if none was specified; the whitespace in the public identifier
will have been normalized as required by the XML spec.
The openEntityNames argument is a space-separated list of the names of the entities
that are open for the parse of this entity (including the name of the referenced
entity); this can be passed as the openEntityNames argument to
XML_ExternalEntityParserCreate; openEntityNames is valid only until the handler
returns, so if the referenced entity is to be parsed later, it must be copied.
The handler should return 0 if processing should not continue because of
a fatal error in the handling of the external entity.
In this case the calling parser will return an XML_ERROR_EXTERNAL_ENTITY_HANDLING
error.
Note that unlike other handlers the first argument is the parser, not userData. */

typedef int (*XML_ExternalEntityRefHandler)(XML_Parser parser,
					    const XML_Char *openEntityNames,
					    const XML_Char *base,
					    const XML_Char *systemId,
					    const XML_Char *publicId);

/* This structure is filled in by the XML_UnknownEncodingHandler
to provide information to the parser about encodings that are unknown
to the parser.
The map[b] member gives information about byte sequences
whose first byte is b.
If map[b] is c where c is >= 0, then b by itself encodes the Unicode scalar value c.
If map[b] is -1, then the byte sequence is malformed.
If map[b] is -n, where n >= 2, then b is the first byte of an n-byte
sequence that encodes a single Unicode scalar value.
The data member will be passed as the first argument to the convert function.
The convert function is used to convert multibyte sequences;
s will point to a n-byte sequence where map[(unsigned char)*s] == -n.
The convert function must return the Unicode scalar value
represented by this byte sequence or -1 if the byte sequence is malformed.
The convert function may be null if the encoding is a single-byte encoding,
that is if map[b] >= -1 for all bytes b.
When the parser is finished with the encoding, then if release is not null,
it will call release passing it the data member;
once release has been called, the convert function will not be called again.

Expat places certain restrictions on the encodings that are supported
using this mechanism.

1. Every ASCII character that can appear in a well-formed XML document,
other than the characters

  $@\^`{}~

must be represented by a single byte, and that byte must be the
same byte that represents that character in ASCII.

2. No character may require more than 4 bytes to encode.

3. All characters encoded must have Unicode scalar values <= 0xFFFF,
(ie characters that would be encoded by surrogates in UTF-16
are  not allowed).  Note that this restriction doesn't apply to
the built-in support for UTF-8 and UTF-16.

4. No Unicode character may be encoded by more than one distinct sequence
of bytes. */

typedef struct {
  int map[256];
  void *data;
  int (*convert)(void *data, const char *s);
  void (*release)(void *data);
} XML_Encoding;

/* This is called for an encoding that is unknown to the parser.
The encodingHandlerData argument is that which was passed as the
second argument to XML_SetUnknownEncodingHandler.
The name argument gives the name of the encoding as specified in
the encoding declaration.
If the callback can provide information about the encoding,
it must fill in the XML_Encoding structure, and return 1.
Otherwise it must return 0.
If info does not describe a suitable encoding,
then the parser will return an XML_UNKNOWN_ENCODING error. */

typedef int (*XML_UnknownEncodingHandler)(void *encodingHandlerData,
					  const XML_Char *name,
					  XML_Encoding *info);

void XMLPARSEAPI
XML_SetElementHandler(XML_Parser parser,
		      XML_StartElementHandler start,
		      XML_EndElementHandler end);

void XMLPARSEAPI
XML_SetCharacterDataHandler(XML_Parser parser,
			    XML_CharacterDataHandler handler);

void XMLPARSEAPI
XML_SetProcessingInstructionHandler(XML_Parser parser,
				    XML_ProcessingInstructionHandler handler);

void XMLPARSEAPI
XML_SetDefaultHandler(XML_Parser parser,
		      XML_DefaultHandler handler);

void XMLPARSEAPI
XML_SetUnparsedEntityDeclHandler(XML_Parser parser,
				 XML_UnparsedEntityDeclHandler handler);

void XMLPARSEAPI
XML_SetNotationDeclHandler(XML_Parser parser,
			   XML_NotationDeclHandler handler);

void XMLPARSEAPI
XML_SetExternalEntityRefHandler(XML_Parser parser,
				XML_ExternalEntityRefHandler handler);

void XMLPARSEAPI
XML_SetUnknownEncodingHandler(XML_Parser parser,
			      XML_UnknownEncodingHandler handler,
			      void *encodingHandlerData);

/* This can be called within a handler for a start element, end element,
processing instruction or character data.  It causes the corresponding
markup to be passed to the default handler.
Within the expansion of an internal entity, nothing will be passed
to the default handler, although this usually will not happen since
setting a default handler inhibits expansion of internal entities. */
void XMLPARSEAPI XML_DefaultCurrent(XML_Parser parser);

/* This value is passed as the userData argument to callbacks. */
void XMLPARSEAPI
XML_SetUserData(XML_Parser parser, void *userData);

/* Returns the last value set by XML_SetUserData or null. */
#define XML_GetUserData(parser) (*(void **)(parser))

/* If this function is called, then the parser will be passed
as the first argument to callbacks instead of userData.
The userData will still be accessible using XML_GetUserData. */

void XMLPARSEAPI
XML_UseParserAsHandlerArg(XML_Parser parser);

/* Sets the base to be used for resolving relative URIs in system identifiers in
declarations.  Resolving relative identifiers is left to the application:
this value will be passed through as the base argument to the
XML_ExternalEntityRefHandler, XML_NotationDeclHandler
and XML_UnparsedEntityDeclHandler. The base argument will be copied.
Returns zero if out of memory, non-zero otherwise. */

int XMLPARSEAPI
XML_SetBase(XML_Parser parser, const XML_Char *base);

const XML_Char XMLPARSEAPI *
XML_GetBase(XML_Parser parser);

/* Parses some input. Returns 0 if a fatal error is detected.
The last call to XML_Parse must have isFinal true;
len may be zero for this call (or any other). */
int XMLPARSEAPI
XML_Parse(XML_Parser parser, const char *s, int len, int isFinal);

void XMLPARSEAPI *
XML_GetBuffer(XML_Parser parser, int len);

int XMLPARSEAPI
XML_ParseBuffer(XML_Parser parser, int len, int isFinal);

/* Creates an XML_Parser object that can parse an external general entity;
openEntityNames is a space-separated list of the names of the entities that are open
for the parse of this entity (including the name of this one);
encoding is the externally specified encoding,
or null if there is no externally specified encoding.
This can be called at any point after the first call to an ExternalEntityRefHandler
so longer as the parser has not yet been freed.
The new parser is completely independent and may safely be used in a separate thread.
The handlers and userData are initialized from the parser argument.
Returns 0 if out of memory.  Otherwise returns a new XML_Parser object. */
XML_Parser XMLPARSEAPI
XML_ExternalEntityParserCreate(XML_Parser parser,
			       const XML_Char *openEntityNames,
			       const XML_Char *encoding);

enum XML_Error {
  XML_ERROR_NONE,
  XML_ERROR_NO_MEMORY,
  XML_ERROR_SYNTAX,
  XML_ERROR_NO_ELEMENTS,
  XML_ERROR_INVALID_TOKEN,
  XML_ERROR_UNCLOSED_TOKEN,
  XML_ERROR_PARTIAL_CHAR,
  XML_ERROR_TAG_MISMATCH,
  XML_ERROR_DUPLICATE_ATTRIBUTE,
  XML_ERROR_JUNK_AFTER_DOC_ELEMENT,
  XML_ERROR_PARAM_ENTITY_REF,
  XML_ERROR_UNDEFINED_ENTITY,
  XML_ERROR_RECURSIVE_ENTITY_REF,
  XML_ERROR_ASYNC_ENTITY,
  XML_ERROR_BAD_CHAR_REF,
  XML_ERROR_BINARY_ENTITY_REF,
  XML_ERROR_ATTRIBUTE_EXTERNAL_ENTITY_REF,
  XML_ERROR_MISPLACED_XML_PI,
  XML_ERROR_UNKNOWN_ENCODING,
  XML_ERROR_INCORRECT_ENCODING,
  XML_ERROR_UNCLOSED_CDATA_SECTION,
  XML_ERROR_EXTERNAL_ENTITY_HANDLING
};

/* If XML_Parse or XML_ParseBuffer have returned 0, then XML_GetErrorCode
returns information about the error. */

enum XML_Error XMLPARSEAPI XML_GetErrorCode(XML_Parser parser);

/* These functions return information about the current parse location.
They may be called when XML_Parse or XML_ParseBuffer return 0;
in this case the location is the location of the character at which
the error was detected.
They may also be called from any other callback called to report
some parse event; in this the location is the location of the first
of the sequence of characters that generated the event. */

int XMLPARSEAPI XML_GetCurrentLineNumber(XML_Parser parser);
int XMLPARSEAPI XML_GetCurrentColumnNumber(XML_Parser parser);
long XMLPARSEAPI XML_GetCurrentByteIndex(XML_Parser parser);

/* For backwards compatibility with previous versions. */
#define XML_GetErrorLineNumber XML_GetCurrentLineNumber
#define XML_GetErrorColumnNumber XML_GetCurrentColumnNumber
#define XML_GetErrorByteIndex XML_GetCurrentByteIndex

/* Frees memory used by the parser. */
void XMLPARSEAPI
XML_ParserFree(XML_Parser parser);

/* Returns a string describing the error. */
const XML_LChar XMLPARSEAPI *XML_ErrorString(int code);

#ifdef __cplusplus
}
#endif

#endif /* not XmlParse_INCLUDED */
