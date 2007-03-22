/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ex: set tabstop=8 softtabstop=4 shiftwidth=4 expandtab: */
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
 * The Original Code is developed for mozilla.
 *
 * The Initial Developer of the Original Code is
 * Kenneth Herron <kherron@fastmail.us>.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Roland Mainz <roland.mainz@nrubsig.org>
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
 
#include "nsEPSObjectPS.h"
#include "prprf.h"


/* For details on the EPSF spec, see Adobe specification #5002,
 * "Encapsulated PostScript File Format Specification". The document
 * structuring conventions are described in Specificion #5001, 
 * "PostScript Language Document Structuring Conventions Specification".
 */


/** ------------------------------------------------------------------
 * Constructor
 */

nsEPSObjectPS::nsEPSObjectPS(FILE *aFile) :
  mStatus(NS_ERROR_INVALID_ARG),
  mEPSF(aFile),
  mBBllx(0.0),
  mBBlly(0.0),
  mBBurx(0.0),
  mBBury(0.0)
{
  NS_PRECONDITION(aFile != nsnull,   "aFile == nsnull");
  NS_PRECONDITION(0 == fseek(aFile, 0L, SEEK_SET), "File isn't seekable");    
  Parse();
}

/** ------------------------------------------------------------------
 * Read one line from the file handle into the buffer, following rules
 * for EPS data. The line terminator is not copied into the buffer.
 *
 * EPS file lines must be less than 256 characters, not including
 * line terminators. Lines may be terminated by CR, LF, CRLF, or LFCR.
 * See EPSF spec, section 2.9, "Miscellaneous Constraints".
 *
 * @param aBuffer Buffer in which to place the EPSF text.
 * @param aBufSiz Size of aBuffer
 * @param aSrc    FILE opened for reading
 * @return PR_TRUE   if a line could be read into aBuffer successfully.
 *         PR_FALSE  if EOF or an I/O error was encountered without reading
 *                   any data, or if the line being read is too large for
 *                   the buffer.
 */

PRBool
nsEPSObjectPS::EPSFFgets(nsACString& aBuffer)
{
  aBuffer.Truncate();
  while (1) {
    int ch = getc(mEPSF);
    if ('\n' == ch) {
      /* Eat any following carriage return */
      ch = getc(mEPSF);
      if ((EOF != ch) && ('\r' != ch))
        ungetc(ch, mEPSF);
      return PR_TRUE;
    }
    else if ('\r' == ch) {
      /* Eat any following line feed */
      ch = getc(mEPSF);
      if ((EOF != ch) && ('\n' != ch))
        ungetc(ch, mEPSF);
      return PR_TRUE;
    }
    else if (EOF == ch) {
      /* If we read any text before the EOF, return true. */
      return !aBuffer.IsEmpty();
    }

    /* Normal case */
    aBuffer.Append((char)ch);
  }
}


/** ------------------------------------------------------------------
 * Parse the EPSF and initialize the object accordingly.
 */

void
nsEPSObjectPS::Parse()
{
  nsCAutoString line;

  NS_PRECONDITION(nsnull != mEPSF, "No file");

  rewind(mEPSF);
  while (EPSFFgets(line)) {
    if (PR_sscanf(line.get(), "%%%%BoundingBox: %lf %lf %lf %lf",
                  &mBBllx, &mBBlly, &mBBurx, &mBBury) == 4) {
      mStatus = NS_OK;
      return;
    }
  }
  mStatus = NS_ERROR_INVALID_ARG;
}


/** ------------------------------------------------------------------
 * Write the EPSF to the specified file handle.
 * @return NS_OK if the entire EPSF was written without error, or
 *         else a suitable error code.
 */
nsresult
nsEPSObjectPS::WriteTo(FILE *aDest)
{
  NS_PRECONDITION(NS_SUCCEEDED(mStatus), "Bad status");

  nsCAutoString line;
  PRBool        inPreview = PR_FALSE;

  rewind(mEPSF);
  while (EPSFFgets(line)) {
    if (inPreview) {
      /* filter out the print-preview section */
      if (StringBeginsWith(line, NS_LITERAL_CSTRING("%%EndPreview")))
          inPreview = PR_FALSE;
      continue;
    }
    else if (StringBeginsWith(line, NS_LITERAL_CSTRING("%%BeginPreview:"))){
      inPreview = PR_TRUE;
      continue;
    }

    /* Output the EPSF with this platform's line terminator */
    fwrite(line.get(), line.Length(), 1, aDest);
    putc('\n', aDest);
  }
  return NS_OK;
}


