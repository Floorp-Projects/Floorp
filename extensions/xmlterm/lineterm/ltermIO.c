/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "MPL"); you may not use this file
 * except in compliance with the MPL. You may obtain a copy of
 * the MPL at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the MPL is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the MPL for the specific language governing
 * rights and limitations under the MPL.
 * 
 * The Original Code is lineterm.
 * 
 * The Initial Developer of the Original Code is Ramalingam Saravanan.
 * Portions created by Ramalingam Saravanan <svn@xmlterm.org> are
 * Copyright (C) 1999 Ramalingam Saravanan. All Rights Reserved.
 * 
 * Contributor(s):
 * 
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU General Public License (the "GPL"), in which case
 * the provisions of the GPL are applicable instead of
 * those above. If you wish to allow use of your version of this
 * file only under the terms of the GPL and not to allow
 * others to use your version of this file under the MPL, indicate
 * your decision by deleting the provisions above and replace them
 * with the notice and other provisions required by the GPL.
 * If you do not delete the provisions above, a recipient
 * may use your version of this file under either the MPL or the
 * GPL.
 */

/* ltermIO.c: LTERM PTY input/output data processing
 */

/* public declarations */
#include "lineterm.h"

/* private declarations */
#include "ltermPrivate.h"


static int ltermWrite(struct lterms *lts, int *opcodes);

static int ltermReturnStreamData(struct lterms *lts, struct LtermRead *ltr);
static int ltermReturnScreenData(struct lterms *lts, struct LtermRead *ltr,
                                 int opcodes, int opvals, int oprow);
static int ltermReturnInputLine(struct lterms *lts, struct LtermRead *ltr,
                                int completionRequested);
static int ltermReturnOutputLine(struct lterms *lts, struct LtermRead *ltr);


/** Transmits input line + optional control character UCH to child process.
 * If UCH is 0, no control character is transmitted.
 * Copy of the transmitted plain text (without any markup or character escapes)
 * is saved in the echo buffer, prefixed by the prompt.
 * If echoControl is true, control character is echoed in printable form.
 * If completionCode == LTERM_NO_COMPLETION, transmit complete line
 * and set input line break flag.
 * If completionCode != LTERM_NO_COMPLETION, transmit only characters to
 * left of current and set completion request flag.
 * Called from ltermLineInput.
 * @return 0 on success, -1 on error.
 */
int ltermSendLine(struct lterms *lts, UNICHAR uch,
                  int echoControl, int completionCode)
{
  struct LtermInput *lti = &(lts->ltermInput);
  struct LtermOutput *lto = &(lts->ltermOutput);
  UNICHAR insch;
  int glyphCount, prefixChars, charCount, sendCount;
  int j, k, startCol, nCols, charIndex;

  LTERM_LOG(ltermSendLine,40,
       ("uch=0x%x, echoControl=%d, completionCode=%d, completionRequest=%d\n",
         uch, echoControl, completionCode, lts->completionRequest));

  if ((lts->completionRequest == LTERM_HISTORY_COMPLETION) &&
      (lts->completionChars > 0)) {
    /* Delete prior completion inserts */
    if (ltermDeleteGlyphs(lti, lts->completionChars) != 0)
      return -1;
  }

  if (completionCode != LTERM_NO_COMPLETION) {
    /* Send only glyphs to left of cursor */
    glyphCount = lti->inputCursorGlyph;

  } else {
    /* Send all glyphs in input line */
    glyphCount = lti->inputGlyphs;
  }

  if (lto->promptChars > 0) {
    /* Insert output prompt into echo buffer */
    prefixChars = lto->promptChars;
  } else {
    /* Insert entire output line into echo buffer */
    prefixChars = lto->outputChars;
  }

  charCount = prefixChars;

  LTERM_LOG(ltermSendLine,42,
            ("lto->promptChars=%d, outputChars=%d, glyphCount=%d\n",
             lto->promptChars, lto->outputChars, glyphCount));

  if (charCount >= MAXCOLM1) {
    /* Buffer overflow; error return */
    LTERM_ERROR("ltermSendLine: Error - character buffer overflow\n");
    return -1;
  }

  for (j=0; j<charCount; j++)
    lts->echoLine[j] = lto->outputLine[j];

  /* Append input line */
  for (j=0; j<glyphCount; j++) {
    startCol = lti->inputGlyphColIndex[j];
    nCols = lti->inputGlyphColIndex[j+1] - startCol;

    for (k=startCol; k<startCol+nCols; k++) {
      charIndex = lti->inputColCharIndex[k];

      insch = lti->inputLine[charIndex];

#if 0
      /* COMMENTED OUT: Plain text not escaped; use later in HTML insert */
      if (insch == U_AMPERSAND) {
        /* Escaped character */

        /* Assert that there is one free character position in buffer */
        assert(lti->inputChars < MAXCOL);

        /* Insert null character at the end of the input buffer */
        lti->inputLine[lti->inputChars] = U_NUL;

        /* Match escape sequence against known escape sequences */
        for (m=0; m<LTERM_XML_ESCAPES; m++) {
          if (ucsncmp(lti->inputLine+charIndex,
                      ltermGlobal.escapeSeq[m],
                      (unsigned int) ltermGlobal.escapeLen[m]) == 0)
            break;
        }

        if (m < LTERM_XML_ESCAPES) {
          /* Subsitute single character for escape sequence */
          insch = (UNICHAR) ltermGlobal.escapeChars[m];
        } else {
          LTERM_WARNING("ltermSendLine: Warning - unknown XML character escape sequence\n");
        }
      }
#endif /* 0 */

      if (charCount >= MAXCOLM1) {
        /* Buffer overflow; error return */
        LTERM_ERROR("ltermSendLine: Error - character buffer overflow\n");
        return -1;

      } else {
        /* Insert character into echo line buffer */
        lts->echoLine[charCount++] = insch;
      }
    }
  }


  /* Transmit data to process */
  sendCount = charCount - prefixChars;

  if (lts->completionRequest != LTERM_NO_COMPLETION) {
    /* Completion already requested */

    /* Send only control character */
    if (uch != U_NUL) {
      if (ltermSendData(lts, &uch, 1) != 0)
        return -1;
    }

  } else {
    if (uch != U_NUL) {
      /* Send line+control character to child process */

      /* Insert control character at the end of the buffer (temporarily) */
      lts->echoLine[charCount] = uch;

      sendCount++;
    }

    if (ltermSendData(lts, lts->echoLine+prefixChars, sendCount) != 0)
      return -1;
  }

  if (completionCode != LTERM_NO_COMPLETION) {
    /* Request completion */
    lts->completionRequest = completionCode;

    /* No completion text inserted yet */
    lts->completionChars = 0;

  } else {
    /* Set input line break flag;
     * cleared only by output line break processing
     */
    lts->inputLineBreak = 1;
  }

  if (echoControl && (charCount+2 < MAXCOLM1)) {
    /* Echo control character in printable form */
    lts->echoLine[charCount++] = U_CARET;
    lts->echoLine[charCount++] = U_ATSIGN + uch;
  }

  /* Set count of echo characters (excluding control character) */
  lts->echoChars = charCount;

  LTERM_LOG(ltermSendLine,41,("glyphCount=%d, sendCount=%d\n",
                                 glyphCount, sendCount));
  LTERM_LOGUNICODE(ltermSendLine,41,(lts->echoLine, charCount));

  return 0;
}


/** Writes data from input buffer pipe to pseudo-TTY,
 * processing one complete record read from input buffer pipe
 * and returning
 * OPCODES ::= LINEDATA  (  INPUT ( NEWLINE HIDE? )?
 *                        | INPUT META ( COMPLETION | NEWLINE HIDE? ) )
 * NOTE: This call may block to read data from input buffer pipe,
 *       if lts->inputBufRecord is false.
 * Non-zero OPCODES means echoable input line data was processed.
 * Zero OPCODES implies that no echoable input data was processed, i.e.,
 * an incomplete record or screen/output data processing.
 * @return  0 if successful,
 *         -1 on error, and
 *         -2 if pseudo-TTY has been closed.
 */
int ltermWrite(struct lterms *lts, int *opcodes)
{
  struct LtermInput *lti = &(lts->ltermInput);
  struct LtermOutput *lto = &(lts->ltermOutput);
  char *byteBuf = (char *) lti->inputBuf;
  int returnCode, availableChars, processedChars, headerChars, totalChars, j;

  LTERM_LOG(ltermWrite,20, ("inputLineBreak=%d, inputBufRecord=%d\n",
                               lts->inputLineBreak, lts->inputBufRecord));

  *opcodes = 0;

  if (!lts->inputBufRecord) {
    /* Complete record not available in buffer; read data from input pipe */
    int readMax, n_read;

    readMax = (PIPEHEADER+MAXCOL)*sizeof(UNICHAR) - lti->inputBufBytes;

    assert(readMax > 0);

    n_read = READ(lts->readBUFFER, byteBuf + lti->inputBufBytes,
                  (SIZE_T) readMax);

    if (n_read < 0) {
      LTERM_WARNING("ltermWrite: Warning - error %d in reading from input buffer pipe\n", n_read);
      return -1;
    }

    LTERM_LOG(ltermWrite,22,
              ("read %d bytes from input buffer pipe\n", n_read));

    if (n_read == 0) {
      /* End of file */
      LTERM_LOG(ltermWrite,21,("input buffer pipe has been closed\n"));

      /* Suspend LTERM */
      lts->suspended = 1;

      return -2;
    }

    /* Update count of bytes read so far */
    lti->inputBufBytes += n_read;

    LTERM_LOGUNICODE(ltermWrite,12,(lti->inputBuf,
                                    lti->inputBufBytes/sizeof(UNICHAR)));

    /* Check if one or more complete records is now available in buffer */
    lts->inputBufRecord = (lti->inputBufBytes >= (int)sizeof(UNICHAR)) &&
  (lti->inputBufBytes >= (PIPEHEADER+lti->inputBuf[0])*((int)sizeof(UNICHAR)));

    if (!lts->inputBufRecord) {
      /* Incomplete input record; return */
      return 0;
    }
  }

  /* availableChars may be zero; such zero length input data records
   * are used to notify the "input thread" about output processing events,
   * such as the detection of a command line prompt (which would
   * enable the command completion input mode)
   */
  availableChars = lti->inputBuf[0];
  processedChars = 0;

  LTERM_LOG(ltermWrite,22, ("inputBuf[0]=%d, inputBuf[1]=%d\n",
                              lti->inputBuf[0], lti->inputBuf[1]));

  if (lti->inputBuf[1] == LTERM_WRITE_CLOSE_MESSAGE) {
    /* LTERM being closed */
    return -2;
  }

  if (lti->inputBuf[1] == LTERM_WRITE_PLAIN_INPUT) {
    /* Process plain text input record */
    int doNotCancelCompletion = 0;

    if (lti->inputMode >= LTERM3_COMPLETION_MODE) {

      if (availableChars == 1) {
        /* Single control character completion requests */

        doNotCancelCompletion =
         ( (lts->completionRequest == LTERM_TAB_COMPLETION) &&
           (lti->inputBuf[2] == U_TAB) ) ||
         ( (lts->completionRequest == LTERM_HISTORY_COMPLETION) &&
            ((lti->inputBuf[2] == U_CTL_P) || (lti->inputBuf[2] == U_CTL_N)) );

      } else if (availableChars == 3) {
        /* Three character escape sequences for up/down arrow keys */
        /* NOTE: Input CSI escape sequence; may not be portable */

        doNotCancelCompletion =
       ( (lts->completionRequest == LTERM_HISTORY_COMPLETION) &&
         (lti->inputBuf[2] == U_ESCAPE) && (lti->inputBuf[3] == U_LBRACKET) &&
         ((lti->inputBuf[4] == U_A_CHAR) || (lti->inputBuf[4] == U_B_CHAR)) );
      }
    }

    if (!doNotCancelCompletion &&
        (lts->completionRequest != LTERM_NO_COMPLETION)) {
      /* Cancel any completion request */

      if (ltermCancelCompletion(lts) != 0)
        return -1;
    }

    LTERM_LOGUNICODE(ltermWrite,12,(lti->inputBuf+PIPEHEADER, availableChars));

    returnCode = ltermPlainTextInput(lts, lti->inputBuf+PIPEHEADER,
                                     availableChars, opcodes);
    if (returnCode < 0)
      return returnCode;

    processedChars = availableChars;

  } else if (lti->inputBuf[1] == LTERM_WRITE_XML_INPUT) {
    /* Insert complete XML element into input line, after checks */

    if (lts->completionRequest != LTERM_NO_COMPLETION) {
      /* Cancel command completion request */
      if (ltermCancelCompletion(lts) != 0)
        return -1;
    }

    LTERM_WARNING("ltermWrite: Warning - LTERM_WRITE_XML_INPUT not yet implemented\n");

    processedChars = availableChars;

  } else if (lti->inputBuf[1] == LTERM_WRITE_PLAIN_OUTPUT) {
    /* Insert plain text in decoded character buffer */
    int decodeMax = MAXCOLM1 - lto->decodedChars;

    processedChars = availableChars;

    if (processedChars > decodeMax)
      processedChars = decodeMax;

    if (processedChars > 0) {
      /* Copy characters to decoded character buffer */
      int offset;

      if ((lto->decodedChars > 0) &&
          lto->incompleteEscapeSequence &&
          (lto->decodedStyle[0] != LTERM_ALTOUT_STYLE)) {
        /* Move incomplete escape sequence to end of decoded buffer */
        for (j=lto->decodedChars-1; j>=0; j--) {
          lto->decodedOutput[j+processedChars] = lto->decodedOutput[j];
          lto->decodedStyle[j+processedChars] = lto->decodedStyle[j];
        }

        /* Incomplete escape sequence no longer at beginning of buffer */
        lto->incompleteEscapeSequence = 0;

        /* Insert new characters at beginning of decoded buffer */
        offset = 0;

      } else {
        /* Append new characters at the end of the decoded buffer */
        offset = lto->decodedChars;
      }

      /* Insert new characters in decoded buffer, with appropriate offset */
      for (j=0; j<processedChars; j++) {
        lto->decodedOutput[j+offset] = lti->inputBuf[PIPEHEADER+j];
        lto->decodedStyle[j+offset] = LTERM_ALTOUT_STYLE;
      }

      /* Increment decoded character count */
      lto->decodedChars += processedChars;
    }

    returnCode = 0;
  }

  if (processedChars == availableChars) {
    /* Delete current record completely from buffer */
    processedChars += PIPEHEADER;
    headerChars = 0;

  } else {
    /* Delete processed characters from current record */
    headerChars = PIPEHEADER;

    /* Decrement character count in record header */
    lti->inputBuf[0] -= processedChars;
  }

  LTERM_LOG(ltermWrite,23, ("processedChars=%d, headerChars=%d\n",
                               processedChars, headerChars));

  /* Shift remaining data in buffer */
  totalChars = lti->inputBufBytes/sizeof(UNICHAR);

  for (j=headerChars; j<(totalChars-processedChars); j++)
    lti->inputBuf[j] = lti->inputBuf[j+processedChars];

  /* Decrement buffer count */
  lti->inputBufBytes -= processedChars*sizeof(UNICHAR);

  /* Check if one or more complete records is now left in buffer */
  lts->inputBufRecord = (lti->inputBufBytes >= (int)sizeof(UNICHAR)) &&
  (lti->inputBufBytes >= (PIPEHEADER+lti->inputBuf[0])*((int)sizeof(UNICHAR)));

  LTERM_LOG(ltermWrite,21,
            ("return opcodes=0x%x, inputBufBytes=%d, inputBufRecord=%d\n",
             *opcodes, lti->inputBufBytes, lts->inputBufRecord));

  return 0;
}


/** Reads a (possibly incomplete) line from LTERM.
 * The LtermRead structure *LTR contains both input and output parameters,
 * returning opcodes values of 
 * OPCODES ::= STREAMDATA NEWLINE? ERROR?
 *                        COOKIESTR? DOCSTREAM? XMLSTREAM? WINSTREAM?
 * if StreamMode data is being returned.
 *
 * OPCODES ::= SCREENDATA BELL? ( OUTPUT | CLEAR | INSERT | DELETE | SCROLL )?
 * if ScreenMode data is being returned.
 *
 * OPCODES ::= LINEDATA BELL? (  CLEAR
 *                             | OUTPUT NEWLINE? )
 * if LineMode data is being returned.
 *
 * (Auxiliary procedure for lterm_read.)
 * @return  0 if successful,
 *         -1 if an error occurred while reading,
 *         -2 if pseudo-TTY has been closed,
 *         -3 if more than MAX_COUNT characters are present in the line
 *            (in this case the first MAX_COUNT characters are returned in BUF,
 *             and the rest are discarded).
 */
int ltermRead(struct lterms *lts, struct LtermRead *ltr, int timeout)
{
  struct LtermInput *lti = &(lts->ltermInput);
  struct LtermOutput *lto = &(lts->ltermOutput);
  int waitTime, nfds_all, pollCode, rePoll;
  int inputAvailable, newOutputAvailable;
  int outOpcodes, outOpvals, outOprow, returnCode;
  int j;

  LTERM_LOG(ltermRead,20,("start outputMode=%d\n", lto->outputMode));

  if (lto->outputMode == LTERM1_SCREEN_MODE) {
    char modifiedRows[81];
    int showRows = (lts->nRows < 80) ? lts->nRows : 80;
    for (j=0; j<showRows; j++) {
      if (lto->modifiedCol[j] > -1)
        modifiedRows[j] = 'M';
      else
        modifiedRows[j] = '.';
    }
    modifiedRows[showRows] = '\0';
    LTERM_LOG(ltermRead,28,("modifiedRows=%s\n", modifiedRows));
  }

  waitTime = timeout;

  if (lts->inputBufRecord) {
    /* Complete input record available in buffer */

    LTERM_LOG(ltermRead,21,("inputBufRecord=%d\n", lts->inputBufRecord));
    waitTime = 0;

  } else if ((lto->decodedChars > 0) && !lto->incompleteEscapeSequence) {
    /* Do not wait if there are decoded characters to be processed that are
       not part of an incomplete ESCAPE sequence */

    LTERM_LOG(ltermRead,21,("decodedChars=%d,incompleteEscapeSequence=%d\n",
                  lto->decodedChars, lto->incompleteEscapeSequence));

    waitTime = 0;

  } else if (lto->outputMode == LTERM1_SCREEN_MODE) {
    /* Screen mode: do not wait if there are any modified rows */
    for (j=0; j<lts->nRows; j++)
      if (lto->modifiedCol[j] >= 0) {
        waitTime = 0;
        break;
      }

    if (j<lts->nRows) {
      LTERM_LOG(ltermRead,21,("j=%d, modifiedCol[j] = %d\n",
                    j, lto->modifiedCol[j]));
    }
  }

  /* If stream mode, do not poll STDERR output */
  if (lto->outputMode == LTERM0_STREAM_MODE)
    nfds_all = POLL_COUNT-1;
  else
    nfds_all = lto->nfds;

  do {
    /* Do not re-poll unless something special occurs */
    rePoll = 0;

    /* Default return values in LtermRead structure */
    ltr->read_count = 0;
    ltr->opcodes = 0;
    ltr->opvals = 0;
    ltr->buf_row = -1;
    ltr->buf_col = -1;
    ltr->cursor_row = -1;
    ltr->cursor_col = -1;

    LTERM_LOG(ltermRead,21,("polling nfds = %d, waitTime = %d\n",
                lto->nfds, waitTime));

    /* Poll all files for data to read */
    pollCode = POLL(lto->pollFD, (SIZE_T) lto->nfds, waitTime);

    if (pollCode == -1) {
#if defined(DEBUG_LTERM) && !defined(USE_NSPR_IO)
      int errcode = errno;
      perror("ltermRead");
#else
      int errcode = 0;
#endif
      LTERM_ERROR( "ltermRead: Error return from poll, code=%d\n", errcode);
      return -1;
    }

    /* Do not wait for polling the second time around */
    waitTime = 0;

    /* Check if input data is available */
    inputAvailable = lts->inputBufRecord ||
                     ( (pollCode > 0) &&
                       (lto->pollFD[POLL_INPUTBUF].POLL_REVENTS != 0) );

    /* Check if STDOUT/STDERR data is available */
    newOutputAvailable = (pollCode > 0) &&
                          ( (lto->pollFD[POLL_STDOUT].POLL_REVENTS != 0) ||
                            (lto->pollFD[POLL_STDERR].POLL_REVENTS != 0) );

    LTERM_LOG(ltermRead,21,
            ("inputAvailable=%d, newOutputAvailable=%d, completionRequest=%d\n",
            inputAvailable, newOutputAvailable, lts->completionRequest));

    if (inputAvailable && (!newOutputAvailable ||
                           (lts->completionRequest == LTERM_NO_COMPLETION)) ) {
      /* Process an input data record; either no new output is available, or
       * output is available but command completion has not been requested
       */
      int inOpcodes;

      returnCode = ltermWrite(lts, &inOpcodes);
      if (returnCode < 0)
        return returnCode;

      if (inOpcodes != 0) {
        /* Echoable input data processed; return input line */
        returnCode = ltermReturnInputLine(lts, ltr, 0);
        if (returnCode < 0)
          return returnCode;

        if (inOpcodes & LTERM_META_CODE) {
          ltr->opcodes |= LTERM_META_CODE;

          if (inOpcodes & LTERM_COMPLETION_CODE)
            ltr->opcodes |= LTERM_COMPLETION_CODE;
        }

        if (inOpcodes & LTERM_NEWLINE_CODE) {
          /* Input line break */
          ltr->opcodes |= LTERM_NEWLINE_CODE;

          if (inOpcodes & LTERM_HIDE_CODE)
            ltr->opcodes |= LTERM_HIDE_CODE;

          /* Clear input line buffer */
          ltermClearInputLine(lts);
        }

        if (lts->disabledInputEcho) {
          /* Input echo is disabled, re-poll */
          rePoll = 1;
          LTERM_LOG(ltermRead,32,("Input echo disabled; RE-POLLING\n\n"));
          continue;
        }

        return 0;
      }

      inputAvailable = 0;
    }

    if (newOutputAvailable) {
      /* Read data from STDOUT/STDERR */
      int outChars;

      outChars = ltermReceiveData(lts, nfds_all==POLL_COUNT);
      if (outChars < 0)
        return outChars;
    }

    if (lto->outputMode == LTERM0_STREAM_MODE) {
      /* Stream mode */
      return ltermReturnStreamData(lts, ltr);
    }

    /* If there is some complete decoded output, process it */
    if ((lto->decodedChars > 0) && !lto->incompleteEscapeSequence) {
      returnCode = ltermProcessOutput(lts, &outOpcodes, &outOpvals, &outOprow);
      if (returnCode < 0)
        return -1;

    } else {
      /* No new output to process */
      if (lto->outputMode == LTERM1_SCREEN_MODE) {
        outOpcodes = LTERM_SCREENDATA_CODE;
        outOpvals = 0;
        outOprow = -1;
      } else {
        /* No new output in line mode; return nothing */
        return 0;
      }
    }

    if (outOpcodes & LTERM_SCREENDATA_CODE) {
      /* Return screen mode output */
      return ltermReturnScreenData(lts, ltr, outOpcodes, outOpvals, outOprow);
    }

    if (outOpcodes & LTERM_LINEDATA_CODE) {

      LTERM_LOG(ltermRead,32,
       ("inputLineBreak=%d, completionRequest=%d, completionChars=%d\n",
        lts->inputLineBreak, lts->completionRequest, lts->completionChars));

      if (outOpcodes & LTERM_OUTPUT_CODE) {
        /* Return output line */
        int echoMatch = 0;

        returnCode = ltermReturnOutputLine(lts, ltr);
        if (returnCode < 0)
          return returnCode;

        LTERM_LOG(ltermRead,33, ("read_count=%d, echoChars=%d\n",
                                 ltr->read_count, lts->echoChars));

        if (lts->inputLineBreak) {
          /* Compare output line to initial portion of echo line */
          echoMatch = 1;

          if ( (ltr->read_count > lts->echoChars) ||
               ((ltr->read_count != lts->echoChars) &&
                (outOpcodes & LTERM_NEWLINE_CODE)) ) {
            echoMatch = 0;
          } else {
            int k;
            for (k=0; k<ltr->read_count; k++) {
              if (ltr->buf[k] != lts->echoLine[k]) {
                echoMatch = 0;
                break;
              }
            }
          }
        }

        if (outOpcodes & LTERM_NEWLINE_CODE) {
          /* Complete output line */
          if (!echoMatch)
            ltr->opcodes |= LTERM_NEWLINE_CODE;

          /* Clear output line buffer */
          ltermClearOutputLine(lts);

          /* Clear input line break flag */
          lts->inputLineBreak = 0;

        } else {
          /* Incomplete output line */

          if (lts->completionRequest != LTERM_NO_COMPLETION) {
            /* Completion requested;
             * Compare first/last part of output line to parts of echo line
             */
            int k;
            int extraChars = ltr->read_count - lts->echoChars;
            int compChars = ltr->read_count;
            int preMatch = 1;

            if (extraChars > 0)
              compChars = lts->echoChars;

            for (k=0; k<compChars; k++) {
              if (ltr->buf[k] != lts->echoLine[k]) {
                preMatch = 0;
                break;
              }
            }

            if (!preMatch) {
              /* No match; cancel completion request */
              if (ltermCancelCompletion(lts) != 0)
                return -1;

            } else if (extraChars >= 0) {
              /* Matched complete echo line and perhaps more */

              if ((extraChars > 0) && (extraChars <= MAXCOLM1)) {
                /* Insert extra characters into input line */
                lts->completionChars = extraChars;

                for (k=0; k<extraChars; k++) {
                  if (ltermInsertChar(lti, ltr->buf[compChars+k]) != 0)
                    return -1;
                }

                LTERM_LOG(ltermRead,32,
                          ("++++++++++++ COMPLETION SUCCESSFUL\n"));

                LTERM_LOGUNICODE(ltermRead,32,
                                 (ltr->buf+compChars, extraChars));
              }

              /* Return updated input line, prefixed with prompt */
              returnCode = ltermReturnInputLine(lts, ltr, 1);
              if (returnCode < 0)
                return returnCode;

            } else {
              /* Partial match; do not echo */
              echoMatch = 1;
            }

          } else if (lto->promptChars == lto->outputChars) {
            /* Incomplete output line with just a prompt */

            LTERM_LOG(ltermRead,32,("Prompt-only line\n"));

            /* Return updated input line, prefixed with prompt */
            returnCode = ltermReturnInputLine(lts, ltr, 0);
            if (returnCode < 0)
              return returnCode;
          }
        }

        if (echoMatch) {
          /* Do not display output line; re-poll */
          rePoll = 1;
          LTERM_LOG(ltermRead,32,("Echo match; RE-POLLING\n\n"));
          continue;
        } else {
          return 0;
        }

      } else {
        /* Line mode data with modifier flags; return just cursor position */
        ltr->opcodes = outOpcodes;
        ltr->opvals = 0;

        /* Copy full screen cursor position */
        ltr->cursor_row = -1;
        ltr->cursor_col = 0;

        ltr->read_count = 0;

        return 0;
      }
    }

  } while (rePoll);

  return -1;
}

/** Interrupts output operations in response to an input TTY interrupt signal
 * @return  0 if successful,
 *         -1 if an error occurred
 */
int ltermInterruptOutput(struct lterms *lts)
{
  struct LtermOutput *lto = &(lts->ltermOutput);

  if (lto->outputMode == LTERM0_STREAM_MODE) {
    /* Abnormally terminate output stream */
    lto->streamOpcodes |= LTERM_ERROR_CODE;
  }

  return 0;
}


/** Returns stream data from STDOUT
 * (Auxiliary procedure for ltermRead.)
 * OPCODES ::= STREAMDATA NEWLINE? ERROR?
 *                        COOKIESTR? DOCSTREAM? XMLSTREAM? WINSTREAM?
 * The LtermRead structure *LTR contains both input and output parameters.
 * @return  0 if successful,
 *         -1 if an error occurred while reading,
 */
static int ltermReturnStreamData(struct lterms *lts, struct LtermRead *ltr)
{
  struct LtermOutput *lto = &(lts->ltermOutput);
  UNICHAR *locTerminator;
  int streamTerminated, charCount, deleteCount, j;

  LTERM_LOG(ltermReturnStreamData,30,("start\n"));

  if (lto->streamOpcodes & LTERM_ERROR_CODE) {
    /* Error in stream output; terminate stream abnormally */
    LTERM_LOG(ltermReturnStreamData,32,("Error termination of STREAM mode\n"));

    if (lto->savedOutputMode == LTERM1_SCREEN_MODE) {
      if (ltermSwitchToScreenMode(lts) != 0)
        return -1;
    } else {
      if (ltermSwitchToLineMode(lts) != 0)
        return -1;
    }

    ltr->opcodes = lto->streamOpcodes | LTERM_STREAMDATA_CODE
                                      | LTERM_NEWLINE_CODE;
    ltr->read_count = 0;
    return 0;

  } else if (lto->decodedChars == 0) {
    /* No output data available; return null opcode */
    return 0;
  }

  ltr->opcodes = LTERM_STREAMDATA_CODE | lto->streamOpcodes;

  if (ucslen(lto->streamTerminator) == 0) {
    /* Null terminated stream */
    locTerminator = NULL;
    for (j=0; j<lto->decodedChars; j++) {
      if (lto->decodedOutput[j] == 0) {
        locTerminator = &lto->decodedOutput[j];
        break;
      }
    }

  } else {
    /* Non-null terminated stream */
    assert(lto->decodedChars < MAXCOL);

    /* Insert terminating null character in decoded buffer */
    lto->decodedOutput[lto->decodedChars] = U_NUL;

    /* There should be no NULs in decoded output */
    assert((int)ucslen(lto->decodedOutput) == lto->decodedChars);

    /* Search for stream terminator string in decoded output */
    locTerminator = ucsstr(lto->decodedOutput, lto->streamTerminator);
  }

  /* Stream termination flag */
  streamTerminated = 0;

  if (locTerminator == NULL) {
    /* No terminator found; return decoded stream data */
    charCount = lto->decodedChars;
    if (charCount > ltr->max_count)
      charCount = ltr->max_count;

  } else {
    /* Count stream portion of decoded output (excluding terminator) */
    charCount = locTerminator - lto->decodedOutput;

    if (charCount > ltr->max_count) {
      /* Return only portion of stream data that fits into output buffer,
       * without terminating stream mode
       */
      charCount = ltr->max_count;

    } else {
      /* Return all remaining stream data, terminate stream mode and
       * revert to saved output mode
       */
      streamTerminated = 1;
      if (lto->savedOutputMode == LTERM1_SCREEN_MODE) {
        if (ltermSwitchToScreenMode(lts) != 0)
          return -1;
      } else {
        if (ltermSwitchToLineMode(lts) != 0)
          return -1;
      }
      LTERM_LOG(ltermReturnStreamData,32,("terminating STREAM mode\n"));
    }
  }

  /* Copy stream data to output buffer */
  for (j=0; j<charCount; j++) {
    ltr->buf[j] = lto->decodedOutput[j];
    ltr->style[j] = LTERM_STDOUT_STYLE;
  }

  /* Count of characters read */
  ltr->read_count = charCount;

  /* Count of characters to be deleted */
  deleteCount = charCount;

  if (streamTerminated) {
    /* Stream terminated */
    ltr->opcodes |= LTERM_NEWLINE_CODE;

    /* Delete terminator string from decoded buffer */
    if (ucslen(lto->streamTerminator) == 0) {
      deleteCount += 1;
    } else {
      deleteCount += ucslen(lto->streamTerminator);
    }
  }

  /* Shift remaining characters in decoded buffer */
  for (j=deleteCount; j<lto->decodedChars; j++)
    lto->decodedOutput[j-deleteCount] = lto->decodedOutput[j];

  lto->decodedChars -= deleteCount;

  LTERM_LOG(ltermReturnStreamData,31,("returning STREAM data (%d bytes)\n",
                                         ltr->read_count));

  return 0;
}


/** Returns processed screen data
 * (Auxiliary procedure for ltermRead.)
 * The LtermRead structure *LTR contains both input and output parameters.
 * @return  0 if successful,
 *         -1 if an error occurred while reading,
 *         -3 if more than COUNT characters are present in the line
 *            (in this case the first COUNT characters are returned in BUF,
 *             and the rest are discarded).
 */
static int ltermReturnScreenData(struct lterms *lts, struct LtermRead *ltr,
                                 int opcodes, int opvals, int oprow)
{
  struct LtermOutput *lto = &(lts->ltermOutput);
  int cursorMoved, returnRow, charCount, returnCode;
  int jOffset, j;

  cursorMoved = (lto->returnedCursorRow != lto->cursorRow) ||
                (lto->returnedCursorCol != lto->cursorCol);

  lto->returnedCursorRow = lto->cursorRow;
  lto->returnedCursorCol = lto->cursorCol;

  LTERM_LOG(ltermReturnScreenData,30,("cursorMoved=%d\n", cursorMoved));

  /* Screen mode data with possible modifier flags */
  ltr->opcodes = opcodes;
  ltr->opvals = opvals;

  /* Copy full screen cursor position */
  ltr->cursor_row = lto->returnedCursorRow;
  ltr->cursor_col = lto->returnedCursorCol;

  if (opcodes &
      (LTERM_CLEAR_CODE|LTERM_INSERT_CODE|LTERM_DELETE_CODE|LTERM_SCROLL_CODE)) {
    /* Screen modifier flags; return no character data */
    if (oprow >= 0) {
      ltr->buf_row = oprow;
    } else {
      ltr->buf_row = lto->cursorRow;
    }

    ltr->buf_col = 0;

    ltr->read_count = 0;

    return 0;
  }

  /* Check if any row has been modified */
  returnRow = -1;

  for (j=0; j<lts->nRows; j++)
    if (lto->modifiedCol[j] >= 0) {
      returnRow = j;
      break;
    }

  if (returnRow < 0) {
    /* No modified rows */
    ltr->read_count = 0;
    ltr->buf_row = 0;
    ltr->buf_col = 0;

    if (!cursorMoved && (ltr->opcodes == LTERM_SCREENDATA_CODE)) {
      /* Return no data (***IMPORTANT ACTION, TO PREVENT LOOPING***) */
      ltr->opcodes = 0;
    }

    return 0;
  }

  /* Returning modified row */
  ltr->opcodes |= LTERM_OUTPUT_CODE;

  /* Copy entire row */
  ltr->buf_row = returnRow;
  ltr->buf_col = 0;

  charCount = lts->nCols;

  if (charCount <= ltr->max_count) {
    returnCode = 0;
  } else {
    /* Too much data to fit into output buffer; truncate */
    charCount = ltr->max_count;
    returnCode = -3;
  }

  jOffset = ltr->buf_row * lts->nCols + ltr->buf_col;
  for (j=0; j<charCount; j++) {
    ltr->buf[j]   = lto->screenChar[jOffset+j];
    ltr->style[j] = lto->screenStyle[jOffset+j];
  }

  /* Clear modified flag in returned row */
  lto->modifiedCol[returnRow] = -1;

  ltr->read_count = charCount;

  LTERM_LOG(ltermReturnScreenData,31,("returning SCREEN data\n"));
  LTERM_LOGUNICODE(ltermReturnScreenData,31,(ltr->buf, ltr->read_count));

  return returnCode;
}


/** Returns incomplete input line, prefixed with prompt or incomplete output.
 * (Auxiliary procedure for ltermRead.)
 * The LtermRead structure *LTR contains both input and output parameters.
 * Set COMPLETIONREQUESTED to true if this is a completed command line.
 * @return  0 if successful,
 *         -1 if an error occurred while reading,
 *         -3 if more than COUNT characters are present in the line
 *            (in this case the first COUNT characters are returned in BUF,
 *             and the rest are discarded).
 */
static int ltermReturnInputLine(struct lterms *lts, struct LtermRead *ltr,
                                int completionRequested)
{
  struct LtermOutput *lto = &(lts->ltermOutput);
  struct LtermInput *lti = &(lts->ltermInput);
  int outChars, charCount, inputCursorCol, returnCode;
  int j;

  LTERM_LOG(ltermReturnInputLine,30, ("outputChars=%d, promptChars=%d\n",
                                        lto->outputChars, lto->promptChars));

  if (lto->promptChars > 0) {
    /* Prefix with prompt output data */
    ltr->opcodes = LTERM_LINEDATA_CODE;

    outChars = lto->promptChars;

    if (!completionRequested) {
      /* Hack to handle misidentified prompts;
       *  if output characters following the prompt differ from input line,
       *  display them.
       */
      for (j=0; j<lti->inputChars; j++) {
        if (((j+outChars) < lto->outputChars) && 
            (lto->outputLine[j+outChars] != lti->inputLine[j])) {
          outChars = lto->outputChars;
          break;
        }
      }
    }

    if (outChars > ltr->max_count)
      outChars = 0;

    if (outChars > 0) {
      ltr->opcodes |= LTERM_PROMPT_CODE;
      for (j=0; j<outChars; j++) {
        ltr->buf[j] = lto->outputLine[j];
        ltr->style[j] = LTERM_PROMPT_STYLE;
      }
    }

  } else {
    /* Prefix with entire output line */
    returnCode = ltermReturnOutputLine(lts, ltr);
    if (returnCode < 0)
      return returnCode;

    outChars = ltr->read_count;
  }

  ltr->opcodes |= LTERM_INPUT_CODE;

  charCount = outChars + lti->inputChars;

  if (charCount <= ltr->max_count) {
    returnCode = 0;
  } else {
    /* Too much data to fit into output buffer; truncate */
    charCount = ltr->max_count;
    returnCode = -3;
  }

  /* Append STDIN data, with markup and escape sequences */
  for (j=outChars; j<charCount; j++) {
    ltr->buf[j] = lti->inputLine[j-outChars];
    ltr->style[j] = LTERM_STDIN_STYLE;
  }

  inputCursorCol = lti->inputGlyphColIndex[lti->inputCursorGlyph];

  ltr->buf_row = -1;
  ltr->buf_col = 0;

  ltr->cursor_row = -1;
  ltr->cursor_col = outChars + lti->inputColCharIndex[inputCursorCol];

  ltr->read_count = charCount;

  LTERM_LOG(ltermReturnInputLine,32,("returning INPUT LINE data\n"));
  LTERM_LOGUNICODE(ltermReturnInputLine,32,(ltr->buf, ltr->read_count));

  return returnCode;
}


/** Returns output line data
 * (Auxiliary procedure for ltermRead.)
 * The LtermRead structure *LTR contains both input and output parameters.
 * @return  0 if successful,
 *         -1 if an error occurred while reading,
 *         -3 if more than COUNT characters are present in the line
 *            (in this case the first COUNT characters are returned in BUF,
 *             and the rest are discarded).
 */
static int ltermReturnOutputLine(struct lterms *lts, struct LtermRead *ltr)
{
  struct LtermOutput *lto = &(lts->ltermOutput);
  int charCount, returnCode, j;

  LTERM_LOG(ltermReturnOutputLine,30,
            ("outputChars=%d, promptChars=%d, CursorChar=%d\n",
             lto->outputChars, lto->promptChars, lto->outputCursorChar));

  ltr->opcodes = LTERM_LINEDATA_CODE;

  charCount = lto->outputChars;

  if (charCount <= ltr->max_count) {
    returnCode = 0;
  } else {
    /* Too much data to fit into output buffer; truncate */
    charCount = ltr->max_count;
    returnCode = -3;
  }

  /* Copy output characters (plain text; no markup or escape sequences) */
  ltr->opcodes |= LTERM_OUTPUT_CODE;
  for (j=0; j<charCount; j++) {
    ltr->buf[j] = lto->outputLine[j];
    ltr->style[j] = lto->outputStyle[j];
  }

  if ((lto->promptChars > 0) && (lto->promptChars <= charCount)) {
    /* Set prompt style */
    ltr->opcodes |= LTERM_PROMPT_CODE;
    for (j=0; j<lto->promptChars; j++) {
      ltr->style[j] = LTERM_PROMPT_STYLE;
    }
  }

  ltr->buf_row = -1;
  ltr->buf_col = 0;

  ltr->cursor_row = -1;
  ltr->cursor_col = lto->outputCursorChar;

  ltr->read_count = charCount;

  LTERM_LOG(ltermReturnOutputLine,31,("returning OUTPUT LINE data\n"));
  LTERM_LOGUNICODE(ltermReturnOutputLine,31,(ltr->buf, ltr->read_count));

  return returnCode;
}
