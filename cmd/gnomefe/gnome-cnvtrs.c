/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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

#include "structs.h"
#include "ntypes.h"
#include "net.h"

void
fe_RegisterConverters (void)
{
#ifdef NEW_DECODERS
  NET_ClearAllConverters ();
#endif /* NEW_DECODERS */

#if 0
  if (fe_encoding_extensions)
    {
      int i = 0;
      while (fe_encoding_extensions [i])
	free (fe_encoding_extensions [i++]);
      free (fe_encoding_extensions);
      fe_encoding_extensions = 0;
    }

  /* register X specific decoders
   */
  if (fe_globalData.encoding_filters)
    {
      char *copy = strdup (fe_globalData.encoding_filters);
      char *rest = copy;
      char *end = rest + strlen (rest);

      int exts_count = 0;
      int exts_size = 10;
      char **all_exts = (char **) malloc (sizeof (char *) * exts_size);

      while (rest < end)
	{
	  char *start;
	  char *eol, *colon;
	  char *input, *output, *extensions, *command;
	  eol = strchr (rest, '\n');
	  if (eol) *eol = 0;

	  rest = fe_StringTrim (rest);
	  if (! *rest)
	    /* blank lines are ok */
	    continue;

	  start = rest;

	  colon = strchr (rest, ':');
	  if (! colon) goto LOSER;
	  *colon = 0;
	  input = fe_StringTrim (rest);
	  rest = colon + 1;

	  colon = strchr (rest, ':');
	  if (! colon) goto LOSER;
	  *colon = 0;
	  output = fe_StringTrim (rest);
	  rest = colon + 1;

	  colon = strchr (rest, ':');
	  if (! colon) goto LOSER;
	  *colon = 0;
	  extensions = fe_StringTrim (rest);
	  rest = colon + 1;

	  command = fe_StringTrim (rest);
	  rest = colon + 1;
	  
	  if (*command)
	    {
	      /* First save away the extensions. */
	      char *rest = extensions;
	      while (*rest)
		{
		  char *start;
		  char *comma, *end;
		  while (isspace (*rest))
		    rest++;
		  start = rest;
		  comma = XP_STRCHR (start, ',');
		  end = (comma ? comma - 1 : start + strlen (start));
		  while (end >= start && isspace (*end))
		    end--;
		  if (comma) end++;
		  if (start < end)
		    {
		      all_exts [exts_count] =
			(char *) malloc (end - start + 1);
		      strncpy (all_exts [exts_count], start, end - start);
		      all_exts [exts_count][end - start] = 0;
		      if (++exts_count == exts_size)
			all_exts = (char **)
			  realloc (all_exts,
				   sizeof (char *) * (exts_size += 10));
		    }
		  rest = (comma ? comma + 1 : end);
		}
	      all_exts [exts_count] = 0;
	      fe_encoding_extensions = all_exts;

	      /* Now register the converter. */
	      NET_RegisterExternalDecoderCommand (input, output, command);
	    }
	  else
	    {
	  LOSER:
	      fprintf (stderr,
				   XP_GetString(XFE_COMMANDS_UNPARSABLE_ENCODING_FILTER_SPEC),
				   fe_progname, start);
	    }
	  rest = (eol ? eol + 1 : end);
	}
      free (copy);
    }
#endif

  /* Register standard decoders
     This must come AFTER all calls to NET_RegisterExternalDecoderCommand(),
     (at least in the `NEW_DECODERS' world.)
   */
  NET_RegisterMIMEDecoders ();

#if 0 /* XXX toshok - for now. */
  /* How to save to disk. */
  NET_RegisterContentTypeConverter ("*", FO_SAVE_AS, NULL,
				    fe_MakeSaveAsStream);

  /* Saving any binary format as type `text' should save as `source' instead.
   */
  NET_RegisterContentTypeConverter ("*", FO_SAVE_AS_TEXT, NULL,
				    fe_MakeSaveAsStreamNoPrompt);
  NET_RegisterContentTypeConverter ("*", FO_QUOTE_MESSAGE, NULL,
				    fe_MakeSaveAsStreamNoPrompt);

  /* default presentation converter - offer to save unknown types. */
  NET_RegisterContentTypeConverter ("*", FO_PRESENT, NULL,
				    fe_MakeSaveAsStream);
#endif

#ifndef NO_MOCHA_CONVERTER_HACK
  /* libmocha:LM_InitMocha() installs this convert. We blow away all
   * converters that were installed and hence these mocha default converters
   * dont get recreated. And mocha has no call to re-register them.
   * So this hack. - dp/brendan
   */
  NET_RegisterContentTypeConverter(APPLICATION_JAVASCRIPT, FO_PRESENT, 0,
				   NET_CreateMochaConverter);
#endif /* NO_MOCHA_CONVERTER_HACK */

  /* Parse stuff out of the .mime.types and .mailcap files.
   * We dont have to check dates of files for modified because all that
   * would have been done by the caller. The only place time checking
   * happens is
   * (1) Helperapp page is created
   * (2) Helpers are being saved (OK button pressed on the General Prefs).
   */
  NET_InitFileFormatTypes (NULL /* XXX fe_globalPrefs.private_mime_types_file */,
                           NULL /* XXX fe_globalPrefs.global_mime_types_file*/);
#if 0 /* XXX toshok - for now. */
  fe_isFileChanged(fe_globalPrefs.private_mime_types_file, 0,
		   &fe_globalData.privateMimetypeFileModifiedTime);

  NET_RegisterConverters (fe_globalPrefs.private_mailcap_file,
			  fe_globalPrefs.global_mailcap_file);
  fe_isFileChanged(fe_globalPrefs.private_mailcap_file, 0,
		   &fe_globalData.privateMailcapFileModifiedTime);

  fe_RegisterPrefConverters();

#ifndef NO_WEB_FONTS
  /* Register webfont converters */
  NF_RegisterConverters();
#endif /* NO_WEB_FONTS */

  /* Plugins go on top of all this */
  fe_RegisterPluginConverters();
#endif
}
