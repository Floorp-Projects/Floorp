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

#include "xmlparse.h"
#include "filemap.h"
#include "codepage.h"

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <fcntl.h>

#ifdef _MSC_VER
#include <io.h>
#endif

#ifdef _POSIX_SOURCE
#include <unistd.h>
#endif

#ifndef O_BINARY
#ifdef _O_BINARY
#define O_BINARY _O_BINARY
#else
#define O_BINARY 0
#endif
#endif

#ifdef _MSC_VER
#include <crtdbg.h>
#endif

#ifdef _DEBUG
#define READ_SIZE 16
#else
#define READ_SIZE (1024*8)
#endif

#ifdef XML_UNICODE
#ifndef XML_UNICODE_WCHAR_T
#error xmlwf requires a 16-bit Unicode-compatible wchar_t 
#endif
#define T(x) L ## x
#define ftprintf fwprintf
#define tfopen _wfopen
#define fputts fputws
#define puttc putwc
#define tcscmp wcscmp
#define tcscpy wcscpy
#define tcscat wcscat
#define tcschr wcschr
#define tcsrchr wcsrchr
#define tcslen wcslen
#define tperror _wperror
#define topen _wopen
#define tmain wmain
#define tremove _wremove
#else /* not XML_UNICODE */
#define T(x) x
#define ftprintf fprintf
#define tfopen fopen
#define fputts fputs
#define puttc putc
#define tcscmp strcmp
#define tcscpy strcpy
#define tcscat strcat
#define tcschr strchr
#define tcsrchr strrchr
#define tcslen strlen
#define tperror perror
#define topen open
#define tmain main
#define tremove remove
#endif /* not XML_UNICODE */

static void characterData(void *userData, const XML_Char *s, int len)
{
  FILE *fp = userData;
  for (; len > 0; --len, ++s) {
    switch (*s) {
    case T('&'):
      fputts(T("&amp;"), fp);
      break;
    case T('<'):
      fputts(T("&lt;"), fp);
      break;
    case T('>'):
      fputts(T("&gt;"), fp);
      break;
    case T('"'):
      fputts(T("&quot;"), fp);
      break;
    case 9:
    case 10:
    case 13:
      ftprintf(fp, T("&#%d;"), *s);
      break;
    default:
      puttc(*s, fp);
      break;
    }
  }
}

/* Lexicographically comparing UTF-8 encoded attribute values,
is equivalent to lexicographically comparing based on the character number. */

static int attcmp(const void *att1, const void *att2)
{
  return tcscmp(*(const XML_Char **)att1, *(const XML_Char **)att2);
}

static void startElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
  int nAtts;
  const XML_Char **p;
  FILE *fp = userData;
  puttc(T('<'), fp);
  fputts(name, fp);

  p = atts;
  while (*p)
    ++p;
  nAtts = (p - atts) >> 1;
  if (nAtts > 1)
    qsort((void *)atts, nAtts, sizeof(XML_Char *) * 2, attcmp);
  while (*atts) {
    puttc(T(' '), fp);
    fputts(*atts++, fp);
    puttc(T('='), fp);
    puttc(T('"'), fp);
    characterData(userData, *atts, tcslen(*atts));
    puttc(T('"'), fp);
    atts++;
  }
  puttc(T('>'), fp);
}

static void endElement(void *userData, const XML_Char *name)
{
  FILE *fp = userData;
  puttc(T('<'), fp);
  puttc(T('/'), fp);
  fputts(name, fp);
  puttc(T('>'), fp);
}

static void processingInstruction(void *userData, const XML_Char *target, const XML_Char *data)
{
  FILE *fp = userData;
  puttc(T('<'), fp);
  puttc(T('?'), fp);
  fputts(target, fp);
  puttc(T(' '), fp);
  fputts(data, fp);
  puttc(T('?'), fp);
  puttc(T('>'), fp);
}

static void defaultCharacterData(XML_Parser parser, const XML_Char *s, int len)
{
  XML_DefaultCurrent(parser);
}

static void defaultStartElement(XML_Parser parser, const XML_Char *name, const XML_Char **atts)
{
  XML_DefaultCurrent(parser);
}

static void defaultEndElement(XML_Parser parser, const XML_Char *name)
{
  XML_DefaultCurrent(parser);
}

static void defaultProcessingInstruction(XML_Parser parser, const XML_Char *target, const XML_Char *data)
{
  XML_DefaultCurrent(parser);
}

static void markup(XML_Parser parser, const XML_Char *s, int len)
{
  FILE *fp = XML_GetUserData(parser);
  for (; len > 0; --len, ++s)
    puttc(*s, fp);
}

static
void metaLocation(XML_Parser parser)
{
  const XML_Char *uri = XML_GetBase(parser);
  if (uri)
    ftprintf(XML_GetUserData(parser), T(" uri=\"%s\""), uri);
  ftprintf(XML_GetUserData(parser),
           T(" byte=\"%ld\" line=\"%d\" col=\"%d\""),
	   XML_GetCurrentByteIndex(parser),
	   XML_GetCurrentLineNumber(parser),
	   XML_GetCurrentColumnNumber(parser));
}

static
void metaStartElement(XML_Parser parser, const XML_Char *name, const XML_Char **atts)
{
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<starttag name=\"%s\""), name);
  metaLocation(parser);
  if (*atts) {
    fputts(T(">\n"), fp);
    do {
      ftprintf(fp, T("<attribute name=\"%s\" value=\""), atts[0]);
      characterData(fp, atts[1], tcslen(atts[1]));
      fputts(T("\"/>\n"), fp);
    } while (*(atts += 2));
    fputts(T("</starttag>\n"), fp);
  }
  else
    fputts(T("/>\n"), fp);
}

static
void metaEndElement(XML_Parser parser, const XML_Char *name)
{
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<endtag name=\"%s\""), name);
  metaLocation(parser);
  fputts(T("/>\n"), fp);
}

static
void metaProcessingInstruction(XML_Parser parser, const XML_Char *target, const XML_Char *data)
{
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<pi target=\"%s\" data=\""), target);
  characterData(fp, data, tcslen(data));
  puttc(T('"'), fp);
  metaLocation(parser);
  fputts(T("/>\n"), fp);
}

static
void metaCharacterData(XML_Parser parser, const XML_Char *s, int len)
{
  FILE *fp = XML_GetUserData(parser);
  fputts(T("<chars str=\""), fp);
  characterData(fp, s, len);
  puttc(T('"'), fp);
  metaLocation(parser);
  fputts(T("/>\n"), fp);
}

static
void metaUnparsedEntityDecl(XML_Parser parser,
			       const XML_Char *entityName,
			       const XML_Char *base,
			       const XML_Char *systemId,
			       const XML_Char *publicId,
			       const XML_Char *notationName)
{
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<entity name=\"%s\""), entityName);
  if (publicId)
    ftprintf(fp, T(" public=\"%s\""), publicId);
  fputts(T(" system=\""), fp);
  characterData(fp, systemId, tcslen(systemId));
  puttc(T('"'), fp);
  ftprintf(fp, T(" notation=\"%s\""), notationName);
  metaLocation(parser);
  fputts(T("/>\n"), fp);
}

static
void metaNotationDecl(XML_Parser parser,
		      const XML_Char *notationName,
		      const XML_Char *base,
		      const XML_Char *systemId,
		      const XML_Char *publicId)
{
  FILE *fp = XML_GetUserData(parser);
  ftprintf(fp, T("<notation name=\"%s\""), notationName);
  if (publicId)
    ftprintf(fp, T(" public=\"%s\""), publicId);
  if (systemId) {
    fputts(T(" system=\""), fp);
    characterData(fp, systemId, tcslen(systemId));
    puttc(T('"'), fp);
  }
  metaLocation(parser);
  fputts(T("/>\n"), fp);
}

typedef struct {
  XML_Parser parser;
  int *retPtr;
} PROCESS_ARGS;

static
void reportError(XML_Parser parser, const XML_Char *filename)
{
  int code = XML_GetErrorCode(parser);
  const XML_Char *message = XML_ErrorString(code);
  if (message)
    ftprintf(stdout, T("%s:%d:%ld: %s\n"),
	     filename,
	     XML_GetErrorLineNumber(parser),
	     XML_GetErrorColumnNumber(parser),
	     message);
  else
    ftprintf(stderr, T("%s: (unknown message %d)\n"), filename, code);
}

static
void processFile(const void *data, size_t size, const XML_Char *filename, void *args)
{
  XML_Parser parser = ((PROCESS_ARGS *)args)->parser;
  int *retPtr = ((PROCESS_ARGS *)args)->retPtr;
  if (!XML_Parse(parser, data, size, 1)) {
    reportError(parser, filename);
    *retPtr = 0;
  }
  else
    *retPtr = 1;
}

static
int isAsciiLetter(XML_Char c)
{
  return (T('a') <= c && c <= T('z')) || (T('A') <= c && c <= T('Z'));
}

static
const XML_Char *resolveSystemId(const XML_Char *base, const XML_Char *systemId, XML_Char **toFree)
{
  XML_Char *s;
  *toFree = 0;
  if (!base
      || *systemId == T('/')
#ifdef WIN32
      || *systemId == T('\\')
      || (isAsciiLetter(systemId[0]) && systemId[1] == T(':'))
#endif
     )
    return systemId;
  *toFree = (XML_Char *)malloc((tcslen(base) + tcslen(systemId) + 2)*sizeof(XML_Char));
  if (!*toFree)
    return systemId;
  tcscpy(*toFree, base);
  s = *toFree;
  if (tcsrchr(s, T('/')))
    s = tcsrchr(s, T('/')) + 1;
#ifdef WIN32
  if (tcsrchr(s, T('\\')))
    s = tcsrchr(s, T('\\')) + 1;
#endif
  tcscpy(s, systemId);
  return *toFree;
}

static
int externalEntityRefFilemap(XML_Parser parser,
			     const XML_Char *openEntityNames,
			     const XML_Char *base,
			     const XML_Char *systemId,
			     const XML_Char *publicId)
{
  int result;
  XML_Char *s;
  const XML_Char *filename;
  XML_Parser entParser = XML_ExternalEntityParserCreate(parser, openEntityNames, 0);
  PROCESS_ARGS args;
  args.retPtr = &result;
  args.parser = entParser;
  filename = resolveSystemId(base, systemId, &s);
  XML_SetBase(entParser, filename);
  if (!filemap(filename, processFile, &args))
    result = 0;
  free(s);
  XML_ParserFree(entParser);
  return result;
}

static
int processStream(const XML_Char *filename, XML_Parser parser)
{
  int fd = topen(filename, O_BINARY|O_RDONLY);
  if (fd < 0) {
    tperror(filename);
    return 0;
  }
  for (;;) {
    int nread;
    char *buf = XML_GetBuffer(parser, READ_SIZE);
    if (!buf) {
      close(fd);
      ftprintf(stderr, T("%s: out of memory\n"), filename);
      return 0;
    }
    nread = read(fd, buf, READ_SIZE);
    if (nread < 0) {
      tperror(filename);
      close(fd);
      return 0;
    }
    if (!XML_ParseBuffer(parser, nread, nread == 0)) {
      reportError(parser, filename);
      close(fd);
      return 0;
    }
    if (nread == 0) {
      close(fd);
      break;;
    }
  }
  return 1;
}

static
int externalEntityRefStream(XML_Parser parser,
			    const XML_Char *openEntityNames,
			    const XML_Char *base,
			    const XML_Char *systemId,
			    const XML_Char *publicId)
{
  XML_Char *s;
  const XML_Char *filename;
  int ret;
  XML_Parser entParser = XML_ExternalEntityParserCreate(parser, openEntityNames, 0);
  filename = resolveSystemId(base, systemId, &s);
  XML_SetBase(entParser, filename);
  ret = processStream(filename, entParser);
  free(s);
  XML_ParserFree(entParser);
  return ret;
}

static
int unknownEncodingConvert(void *data, const char *p)
{
  return codepageConvert(*(int *)data, p);
}

static
int unknownEncoding(void *userData,
		    const XML_Char *name,
		    XML_Encoding *info)
{
  int cp;
  static const XML_Char prefixL[] = T("windows-");
  static const XML_Char prefixU[] = T("WINDOWS-");
  int i;

  for (i = 0; prefixU[i]; i++)
    if (name[i] != prefixU[i] && name[i] != prefixL[i])
      return 0;
  
  cp = 0;
  for (; name[i]; i++) {
    static const XML_Char digits[] = T("0123456789");
    const XML_Char *s = tcschr(digits, name[i]);
    if (!s)
      return 0;
    cp *= 10;
    cp += s - digits;
    if (cp >= 0x10000)
      return 0;
  }
  if (!codepageMap(cp, info->map))
    return 0;
  info->convert = unknownEncodingConvert;
  /* We could just cast the code page integer to a void *,
  and avoid the use of release. */
  info->release = free;
  info->data = malloc(sizeof(int));
  if (!info->data)
    return 0;
  *(int *)info->data = cp;
  return 1;
}

static
void usage(const XML_Char *prog)
{
  ftprintf(stderr, T("usage: %s [-r] [-w] [-x] [-d output-dir] [-e encoding] file ...\n"), prog);
  exit(1);
}

int tmain(int argc, XML_Char **argv)
{
  int i;
  const XML_Char *outputDir = 0;
  const XML_Char *encoding = 0;
  int useFilemap = 1;
  int processExternalEntities = 0;
  int windowsCodePages = 0;
  int outputType = 0;

#ifdef _MSC_VER
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF);
#endif

  i = 1;
  while (i < argc && argv[i][0] == T('-')) {
    int j;
    if (argv[i][1] == T('-') && argv[i][2] == T('\0')) {
      i++;
      break;
    }
    j = 1;
    if (argv[i][j] == T('r')) {
      useFilemap = 0;
      j++;
    }
    if (argv[i][j] == T('x')) {
      processExternalEntities = 1;
      j++;
    }
    if (argv[i][j] == T('w')) {
      windowsCodePages = 1;
      j++;
    }
    if (argv[i][j] == T('m')) {
      outputType = 'm';
      j++;
    }
    if (argv[i][j] == T('c')) {
      outputType = 'c';
      j++;
    }
    if (argv[i][j] == T('d')) {
      if (argv[i][j + 1] == T('\0')) {
	if (++i == argc)
	  usage(argv[0]);
	outputDir = argv[i];
      }
      else
	outputDir = argv[i] + j + 1;
      i++;
    }
    else if (argv[i][j] == T('e')) {
      if (argv[i][j + 1] == T('\0')) {
	if (++i == argc)
	  usage(argv[0]);
	encoding = argv[i];
      }
      else
	encoding = argv[i] + j + 1;
      i++;
    }
    else if (argv[i][j] == T('\0') && j > 1)
      i++;
    else
      usage(argv[0]);
  }
  if (i == argc)
    usage(argv[0]);
  for (; i < argc; i++) {
    FILE *fp = 0;
    XML_Char *outName = 0;
    int result;
    XML_Parser parser = XML_ParserCreate(encoding);
    if (outputDir) {
      const XML_Char *file = argv[i];
      if (tcsrchr(file, T('/')))
	file = tcsrchr(file, T('/')) + 1;
#ifdef WIN32
      if (tcsrchr(file, T('\\')))
	file = tcsrchr(file, T('\\')) + 1;
#endif
      outName = malloc((tcslen(outputDir) + tcslen(file) + 2) * sizeof(XML_Char));
      tcscpy(outName, outputDir);
      tcscat(outName, T("/"));
      tcscat(outName, file);
      fp = tfopen(outName, T("wb"));
      if (!fp) {
	tperror(outName);
	exit(1);
      }
#ifdef XML_UNICODE
      puttc(0xFEFF, fp);
#endif
      XML_SetUserData(parser, fp);
      switch (outputType) {
      case 'm':
	XML_UseParserAsHandlerArg(parser);
	fputts(T("<document>\n"), fp);
	XML_SetElementHandler(parser, metaStartElement, metaEndElement);
	XML_SetProcessingInstructionHandler(parser, metaProcessingInstruction);
	XML_SetCharacterDataHandler(parser, metaCharacterData);
	XML_SetUnparsedEntityDeclHandler(parser, metaUnparsedEntityDecl);
	XML_SetNotationDeclHandler(parser, metaNotationDecl);
	break;
      case 'c':
	XML_UseParserAsHandlerArg(parser);
	XML_SetDefaultHandler(parser, markup);
	XML_SetElementHandler(parser, defaultStartElement, defaultEndElement);
	XML_SetCharacterDataHandler(parser, defaultCharacterData);
	XML_SetProcessingInstructionHandler(parser, defaultProcessingInstruction);
	break;
      default:
	XML_SetElementHandler(parser, startElement, endElement);
	XML_SetCharacterDataHandler(parser, characterData);
	XML_SetProcessingInstructionHandler(parser, processingInstruction);
	break;
      }
    }
    if (windowsCodePages)
      XML_SetUnknownEncodingHandler(parser, unknownEncoding, 0);
    if (!XML_SetBase(parser, argv[i])) {
      ftprintf(stderr, T("%s: out of memory"), argv[0]);
      exit(1);
    }
    if (processExternalEntities)
      XML_SetExternalEntityRefHandler(parser,
	                              useFilemap
				      ? externalEntityRefFilemap
				      : externalEntityRefStream);
    if (useFilemap) {
      PROCESS_ARGS args;
      args.retPtr = &result;
      args.parser = parser;
      if (!filemap(argv[i], processFile, &args))
	result = 0;
    }
    else
      result = processStream(argv[i], parser);
    if (outputDir) {
      if (outputType == 'm')
	fputts(T("</document>\n"), fp);
      fclose(fp);
      if (!result)
	tremove(outName);
      free(outName);
    }
    XML_ParserFree(parser);
  }
  return 0;
}
