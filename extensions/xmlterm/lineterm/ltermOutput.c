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

/* ltermOutput.c: LTERM PTY output processing
 */

/* public declarations */
#include "lineterm.h"

/* private declarations */
#include "ltermPrivate.h"


static int ltermAppendOutput(struct lterms *lts, const char *cbuf, int count,
                             UNISTYLE style, int interleaveCheck,
                             int *interleavedBytes, int rawIncompleteMax,
                             int *rawIncompleteBytes, char *rawIncompleteBuf);
static int ltermDecode(const char *rawBuf, int n_total,
                       UNICHAR* decodedBuf, int decodeMax,
                       int decodeNUL,
                       int *rawIncompleteBytes);
static int ltermPromptLocate(struct lterms *lts);


/** Processes output from decoded output buffer and returns *opcodes:
 * OPCODES ::= SCREENDATA BELL? ( CLEAR | INSERT | DELETE | SCROLL )?
 * if ScreenMode data is being returned.
 * OPCODES ::= LINEDATA BELL? (  CLEAR
 *                             | OUTPUT NEWLINE?)
 * if LineMode data is being returned.
 * OPVALS contains return value(s) for specific screen operations
 *  such as INSERT, DELETE, and SCROLL.
 * OPROW contains the row value for specific screen operations such as
 *  INSERT, DELETE, and SCROLL (or -1, cursor row is to be used).
 * @return 0 on success and -1 on error.
 */
int ltermProcessOutput(struct lterms *lts, int *opcodes, int *opvals,
                       int *oprow)
{
  struct LtermOutput *lto = &(lts->ltermOutput);
  UNICHAR uch;
  UNISTYLE ustyle;
  int charIndex, returnCode, consumedChars, remainingChars, j;
  int bellFlag;

  LTERM_LOG(ltermProcessOutput,30,("lto->outputMode=%d, cursorChar=%d, Chars=%d\n",
                lto->outputMode, lto->outputCursorChar, lto->outputChars));

  LTERM_LOG(ltermProcessOutput,32, ("lts->commandNumber=%d\n",
                                      lts->commandNumber));

  /* Set default returned opcodes, opvals, oprow */
  *opcodes = 0;
  *opvals = 0;
  *oprow = -1;

  charIndex = 0;
  bellFlag = 0;

  while ((*opcodes == 0) && (charIndex < lto->decodedChars)) {
    uch = lto->decodedOutput[charIndex];
    ustyle = lto->decodedStyle[charIndex] | lto->styleMask;

    consumedChars = 1;

    if (uch == U_ESCAPE) {
      /* Process escape sequence */
      int savedOutputMode = lto->outputMode;

      LTERM_LOG(ltermProcessOutput,31,("ESCAPE sequence\n"));

      returnCode = ltermProcessEscape(lts, lto->decodedOutput+charIndex,
                                      lto->decodedChars-charIndex,
                                      lto->decodedStyle+charIndex,
                                      &consumedChars, opcodes, opvals, oprow);
      if (returnCode < 0)
        return -1;

      if (returnCode == 1) {
        /* Incomplete escape sequence */
        lto->incompleteEscapeSequence = 1;
        if (lto->outputMode == LTERM1_SCREEN_MODE)
          *opcodes = LTERM_SCREENDATA_CODE;
        else
          *opcodes = LTERM_LINEDATA_CODE | LTERM_OUTPUT_CODE;
      }

      /* Assert that change in output mode is a loop terminating condition */
      if (lto->outputMode != savedOutputMode)
        assert(*opcodes != 0);

    } else if (lto->outputMode == LTERM1_SCREEN_MODE) {
      /* Screen mode processing */

      if ((uch >= (UNICHAR)U_SPACE) && (uch != (UNICHAR)U_DEL)) {
        /* Printable non-TAB character */

        LTERM_LOG(ltermProcessOutput,39,("Screen mode, printable char - %c\n",
                      (char) uch));

        if (lto->insertMode) {
          /* Insert blank character at cursor position */
          if (ltermInsDelEraseChar(lts, 1, LTERM_INSERT_ACTION) != 0)
            return -1;
        }

        /* Mark column as being modified */
        if ((lto->modifiedCol[lto->cursorRow] == -1) ||
            (lto->modifiedCol[lto->cursorRow] > lto->cursorCol))
          lto->modifiedCol[lto->cursorRow] = lto->cursorCol;

        /* Replace character and style info at current cursor location */
        j = lto->cursorRow*lts->nCols + lto->cursorCol;

        assert(j < (lts->nRows * lts->nCols));

        lto->screenChar[j] = uch;
        lto->screenStyle[j] = ustyle;

        if (lto->cursorCol < lts->nCols-1) {
          /* Move cursor right */
          lto->cursorCol++;
        }

      } else {
        /* Control character */

        switch (uch) {
        case U_BACKSPACE:                 /* Backspace */
          LTERM_LOG(ltermProcessOutput,32,("Screen mode, BACKSPACE\n"));
          if (lto->cursorCol > 0)
            lto->cursorCol--;
          break;

        case U_TAB:                       /* Tab */
          LTERM_LOG(ltermProcessOutput,32,("Screen mode, TAB\n"));
          lto->cursorCol = ((lto->cursorCol/8)+1)*8;
          if (lto->cursorCol > lts->nCols-1)
            lto->cursorCol = lts->nCols-1;
          break;

        case U_BEL:                       /* Bell */
          LTERM_LOG(ltermProcessOutput,32,("************ Screen mode, BELL\n"));
          bellFlag = 1;
          break;

        case U_CRETURN:                   /* Carriage return */
          lto->cursorCol = 0;
          break;

        case U_LINEFEED:                  /* Newline */
          LTERM_LOG(ltermProcessOutput,32,("************ Screen mode, NEWLINE\n\n"));

          *opcodes =  LTERM_SCREENDATA_CODE;

          if (lto->cursorRow > lto->botScrollRow) {
            /* Not bottom scrolling line; simply move cursor down one row */
            lto->cursorRow--;

          } else {
            /* Delete top scrollable line, scrolling up */
            if (ltermInsDelEraseLine(lts, 1, lto->topScrollRow, LTERM_DELETE_ACTION) != 0)
              return -1;
            *opcodes |= LTERM_DELETE_CODE;
            *opvals = 1;
            *oprow = lto->topScrollRow;
          }
          break;

        default:
          /* Ignore other control characters (including NULs) */
          break;
        }
      }

    } else {
      /* Line mode processing */

      if ( ((uch >= (UNICHAR)U_SPACE) && (uch != (UNICHAR)U_DEL)) ||
           (uch == (UNICHAR)U_TAB)) {
        /* Printable/TAB character; replace/insert at current cursor location
           or append to end of line */

        LTERM_LOG(ltermProcessOutput,39,("Line mode, printable char - %c\n",
                      (char) uch));

        if (lto->outputCursorChar == lto->outputChars) {
          /* Append single character to end of line */

          if (lto->outputChars+1 > MAXCOLM1) {
            /* Output buffer overflow; ignore character */
            LTERM_WARNING("ltermProcessOutput: Warning - output line buffer overflow\n");
          }

          lto->outputLine[lto->outputChars] = uch;
          lto->outputStyle[lto->outputChars] = ustyle;
          lto->outputChars++;           /* Insert character in output line */

          lto->outputCursorChar++;      /* Reposition cursor */

        } else {
          /* Replace/insert single character in the middle of line */

          if (lto->insertMode) {
            /* Insert blank character at cursor position */
            if (ltermInsDelEraseChar(lts, 1, LTERM_INSERT_ACTION) != 0)
              return -1;
          }

          /* Overwrite single character in the middle of line */
          lto->outputLine[lto->outputCursorChar] = uch;
          lto->outputStyle[lto->outputCursorChar] = ustyle;

          /* Note modifications */
          if (lto->outputCursorChar < lto->outputModifiedChar)
            lto->outputModifiedChar = lto->outputCursorChar;

          lto->outputCursorChar++;      /* Reposition cursor */
        }

      } else {
        /* Control character */

        switch (uch) {
        case U_BACKSPACE:                /* Backspace */
          LTERM_LOG(ltermProcessOutput,32,("Line mode, BACKSPACE\n"));
          if (lto->outputCursorChar > 0)
            lto->outputCursorChar--;
          break;

        case U_CRETURN:                  /* Carriage return */
          LTERM_LOG(ltermProcessOutput,32,("Line mode, CRETURN\n"));
          lto->outputCursorChar = 0;
          break;

        case U_BEL:                      /* Bell */
          LTERM_LOG(ltermProcessOutput,32,("************ Line mode, BELL\n"));
          bellFlag = 1;
          break;

        case U_CTL_L:                    /* Formfeed; clear line and return */
          LTERM_LOG(ltermProcessOutput,32,("************ Line mode, FORMFEED\n\n"));

          ltermClearOutputLine(lts);
          *opcodes = LTERM_LINEDATA_CODE | LTERM_CLEAR_CODE;
          break;

        case U_LINEFEED:                  /* Newline; return complete line */
          LTERM_LOG(ltermProcessOutput,32,("************ Line mode, NEWLINE\n\n"));

          *opcodes =  LTERM_LINEDATA_CODE
                     | LTERM_OUTPUT_CODE
                     | LTERM_NEWLINE_CODE;
          break;

        default:
          /* Ignore other control characters (including NULs) */
          break;
        }
      }
    }

    /* Increment character index */
    charIndex += consumedChars;
  }

  /* Determine count of unprocessed characters */
  remainingChars = lto->decodedChars - charIndex;

  if (remainingChars > 0) {
    /* Move unprocessed output to beginning of decode buffer */

    LTERM_LOG(ltermProcessOutput,32,("Moved %d chars to beginning of decodedOutput\n", remainingChars));
    for (j=0; j<remainingChars; j++) {
      lto->decodedOutput[j] = lto->decodedOutput[j+charIndex];
      lto->decodedStyle[j] = lto->decodedStyle[j+charIndex];
    }
  }

  /* Update remaining decoded character count */
  lto->decodedChars = remainingChars;

  if (*opcodes == 0) {
    /* All output processed; without any special terminating condition */
    if (lto->outputMode == LTERM1_SCREEN_MODE) {
      /* Full screen mode */
      *opcodes = LTERM_SCREENDATA_CODE;

    } else {
      /* Line mode */
      *opcodes = LTERM_LINEDATA_CODE | LTERM_OUTPUT_CODE;
    }
  }

  /* Set bell code */
  if (bellFlag)
    *opcodes |= LTERM_BELL_CODE;

  if (*opcodes & LTERM_LINEDATA_CODE) {
    /* Returning line mode data; check for prompt */

    if ((lts->commandNumber == 0) ||
        (lto->outputModifiedChar < lto->promptChars)) {
      /* If not command line or if "prompt string" may have been modified,
       * search for prompt
       */
      int promptLen;

      LTERM_LOG(ltermProcessOutput,39,("Prompt? modifiedChar=%d, promptChars=%d\n",
                     lto->outputModifiedChar, lto->promptChars));

      /* Reset modification marker */
      lto->outputModifiedChar = lto->outputChars;

      /* Check if prompt string is present in output */
      promptLen = ltermPromptLocate(lts);

      if (promptLen > 0) {
        /* Prompt string found */
        lto->promptChars = promptLen;

        if (lts->commandNumber == 0) {
          /* Set command number */

          /* New command number */
          if (lts->lastCommandNum == 0xFFFF)
            lts->lastCommandNum = 0;
          lts->lastCommandNum++;

          lts->commandNumber = lts->lastCommandNum;

          LTERM_LOG(ltermProcessOutput,32,
                    ("************ Prompt found; commandNumber=%d\n\n",
                     lts->commandNumber));
        }

      } else {
        /* No prompt string */
        if (lts->commandNumber != 0) {
          /* Unset command number and prompt columns */
          UNICHAR temLine[2] = {0, LTERM_WRITE_PLAIN_INPUT};

          lts->commandNumber = 0;
          lto->promptChars = 0;

          /* Notify "input thread" by writing null input record */
          WRITE(lts->writeBUFFER, temLine, 2*sizeof(UNICHAR));

          LTERM_LOG(ltermProcessOutput,32,
                    ("************  No prompt found; not command line\n\n"));
        }
      }
    }
  }

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
    LTERM_LOG(ltermProcessOutput,38,("modifiedRows=%s\n", modifiedRows));
  }

  LTERM_LOG(ltermProcessOutput,31,("returned opcodes=0x%X\n", *opcodes));

  return 0;
}


/** Clears output line buffer */
void ltermClearOutputLine(struct lterms *lts)
{
  struct LtermOutput *lto = &(lts->ltermOutput);

  LTERM_LOG(ltermClearOutputLine,40,("\n"));

  lto->outputChars = 0;
  lto->outputCursorChar = 0;
  lto->outputModifiedChar = 0;
  lto->promptChars = 0;

  lts->commandNumber = 0;
}


/** Clears output screen buffer (allocating memory, if first time/resized) */
int ltermClearOutputScreen(struct lterms *lts)
{
  struct LtermOutput *lto = &(lts->ltermOutput);
  int j;

  LTERM_LOG(ltermClearOutputScreen,40,("\n"));

  if (lto->screenChar == NULL) {
    /* Allocate memory for full screen */
    int screenSize = lts->nRows * lts->nCols;

    lto->screenChar = (UNICHAR *) MALLOC(screenSize * sizeof(UNICHAR));

    if (lto->screenChar == NULL) {
      LTERM_ERROR("ltermClearOutputScreen: Error - failed to allocate memory for chars\n");
      return -1;
    }

    assert(lto->screenStyle == NULL);
    lto->screenStyle = (UNISTYLE *) MALLOC(screenSize * sizeof(UNISTYLE));

    if (lto->screenStyle == NULL) {
      LTERM_ERROR("ltermClearOutputScreen: Error - failed to allocate memory for style\n");
      return -1;
    }
  }  

  /* Position cursor at home */
  lto->cursorRow = lts->nRows - 1;
  lto->cursorCol = 0;

  /* Blank out entire screen */
  if (ltermInsDelEraseLine(lts, lts->nRows, lts->nRows-1, LTERM_ERASE_ACTION) != 0)
    return -1;

  /* No rows modified yet */
  for (j=0; j<lts->nRows; j++) {
    lto->modifiedCol[j] = -1;
  }

  return 0;
}


/** Saves current output mode value and switches to stream output mode,
 * with specified opcodes and terminator string.
 * @return 0 on success and -1 on error.
 */
int ltermSwitchToStreamMode(struct lterms *lts, int streamOpcodes,
                                    const UNICHAR *streamTerminator)
{
  struct LtermOutput *lto = &(lts->ltermOutput);
  int strLength;

  LTERM_LOG(ltermSwitchToStreamMode,40,("streamOpcodes=0x%x\n",streamOpcodes));

  if (streamTerminator != NULL) {
    /* Save terminator string (may be null) */
    strLength = ucslen(streamTerminator);
    ucsncpy( lto->streamTerminator, streamTerminator, MAXSTREAMTERM);

    LTERM_LOGUNICODE(ltermSwitchToStreamMode,41,(streamTerminator,
                                                 (int) strLength));
  } else {
    /* Null terminator */
    strLength = 0;
    lto->streamTerminator[0] = U_NUL;
  }

  if (strLength > MAXSTREAMTERM-1) {
    LTERM_ERROR("ltermSwitchToStreamMode: Error - terminator string too long\n");
    return -1;
  }

  if (lts->options & LTERM_NONUL_FLAG) {
    /* No decoding of NUL characters */
    if (strLength == 0) {
      LTERM_ERROR("ltermSwitchToStreamMode: Error - null terminator string not allowed\n");
      return -1;
    }
  } else {
    /* Decoding NUL characters */
    if (strLength > 0) {
      LTERM_ERROR("ltermSwitchToStreamMode: Error - terminator string must be NUL\n");
      return -1;
    }
  }

  lto->savedOutputMode = lto->outputMode;
  lto->outputMode = LTERM0_STREAM_MODE;
  lto->streamOpcodes = streamOpcodes;
  return 0;
}


/** Switches to screen output mode.
 * @return 0 on success and -1 on error.
 */
int ltermSwitchToScreenMode(struct lterms *lts)
{
  struct LtermOutput *lto = &(lts->ltermOutput);

  LTERM_LOG(ltermSwitchToScreenMode,40,("\n"));

  if (lto->outputMode == LTERM2_LINE_MODE) {
    /* Switching from line mode to screen mode */

    /* Clear styleMask */
    lto->styleMask = 0;

    /* Clear screen */
    if (ltermClearOutputScreen(lts) != 0)
      return -1;

    /* Reset returned cursor location */
    lto->returnedCursorRow = -1;
    lto->returnedCursorCol = -1;

    /* Scrolling region */
    lto->topScrollRow = lts->nRows-1;
    lto->botScrollRow = 0;

    /* Disable input echo */
    lts->restoreInputEcho = !lts->disabledInputEcho;
    lts->disabledInputEcho = 1;

    /* Switch to raw input mode */
    ltermSwitchToRawMode(lts);

  }

  lto->outputMode = LTERM1_SCREEN_MODE;
  return 0;
}


/** Switches to line output mode.
 * @return 0 on success and -1 on error.
 */
int ltermSwitchToLineMode(struct lterms *lts)
{
  struct LtermOutput *lto = &(lts->ltermOutput);
  int j;

  LTERM_LOG(ltermSwitchToLineMode,40,("\n"));

  if (lto->outputMode == LTERM1_SCREEN_MODE) {
    /* Switching from screen mode to line mode */

    /* Switch to line input mode */
    ltermClearInputLine(lts);

    if (lts->restoreInputEcho) {
      /* Re-enable input echo */
      lts->disabledInputEcho = 0;
      lts->restoreInputEcho = 0;
    }

    /* Clear styleMask */
    lto->styleMask = 0;

    /* Clear output line */
    ltermClearOutputLine(lts);

    /* Copy bottom line to line output buffer ??? */
    lto->outputChars = lts->nCols;

    assert(lts->nCols < MAXCOL);

    for (j=0; j<lts->nCols; j++) {
      lto->outputLine[j] =  lto->screenChar[j];
      lto->outputStyle[j] = lto->screenStyle[j];
    }

  }

  lto->outputMode = LTERM2_LINE_MODE;
  return 0;
}


/** Reads data from process STDOUT and/or STDERR using file descriptors
 * from the LTERM POLL structure, and converts to Unicode, saving
 * incomplete character encodings in corresponding incomplete raw buffers.
 * Decoded characters are appended to the decodedOutput buffer,
 * with appropriate style settings.
 * If READERR is false, STDERR is never read.
 * @return the number of decoded characters appended (>=0) if successful,
 *         -1 if an error occurred during processing, or
 *         -2 if pseudo-TTY has been closed.
 */
int ltermReceiveData(struct lterms *lts, int readERR)
{
  struct LtermOutput *lto = &(lts->ltermOutput);
  char temERRBuf[MAXCOL], temOUTBuf[MAXCOL];
  int readERRMax, readOUTMax;
  int nReadERR, nTotalERR, nReadOUT, nTotalOUT;
  int interleavedBytes, n_decoded, n_decoded_tot, j;

  LTERM_LOG(ltermReceiveData,30,("\n"));

  nTotalERR = 0;
  if (readERR && (lto->pollFD[POLL_STDERR].POLL_REVENTS != 0)) {
    /* Read data from STDERR */

    /* Read maximum number of bytes that will all fit into decodedOutput
       when decoded, assuming number of decoded characters will not exceed
       the number of encoded bytes */

    readERRMax = MAXCOLM1 - lto->decodedChars - lto->rawERRBytes;

    /* Reduce by half to leave some space for STDOUT data */
    readERRMax = readERRMax / 2;

    if (readERRMax <= 0) {
      /* Decoded buffer overflow */
      LTERM_WARNING("ltermReceiveData: Warning - decoded buffer overflow\n");

      /* Non-fatal error recovery; return without reading */
      return 0;
    }

    /* Copy any incomplete raw output to beginning of buffer */
    for (j=0; j<lto->rawERRBytes; j++)
      temERRBuf[j] = lto->rawERRBuf[j];

    /* Read STDERRdata (blocking mode) */
    nReadERR = READ(lto->pollFD[POLL_STDERR].fd,
                    temERRBuf + lto->rawERRBytes, (SIZE_T) readERRMax);

    if (nReadERR < 0) {
#if defined(DEBUG_LTERM) && !defined(USE_NSPR_IO)
      int errcode = errno;
      /*    perror("ltermReceiveData"); */
#else
      int errcode = 0;
#endif
      LTERM_ERROR( "ltermReceiveData: Error %d in reading from process STDERR\n",
                   errcode);
      return -1;
    }

    if (nReadERR == 0) {
      /* End of file => pseudo-TTY has been closed */
      LTERM_LOG(ltermReceiveData,31,("pseudo-TTY has been closed\n"));

      /* Suspend LTERM */
      lts->suspended = 1;

      return -2;
    }


    LTERM_LOG(ltermReceiveData,32,("Read %d raw bytes from STDERR\n", nReadERR));
    nTotalERR = nReadERR + lto->rawERRBytes;
  }

  nTotalOUT = 0;
  if (lto->pollFD[POLL_STDOUT].POLL_REVENTS != 0) {
    /* Read data from STDOUT */

    /* Read maximum number of bytes that will all fit into decodedOutput
       when decoded, assuming number of decoded characters will not exceed
       the number of encoded bytes */
    readOUTMax = MAXCOLM1 - lto->decodedChars - lto->rawOUTBytes - nTotalERR;

    if (readOUTMax <= 0) {
      /* Decoded buffer overflow */
      LTERM_WARNING("ltermReceiveData: Warning - decoded buffer overflow\n");

      /* Non-fatal error recovery; return without reading */
      return 0;
    }

    /* Copy any incomplete raw output to beginning of buffer */
    for (j=0; j<lto->rawOUTBytes; j++)
      temOUTBuf[j] = lto->rawOUTBuf[j];

    /* Read STDOUTdata (blocking mode) */
    nReadOUT = READ(lto->pollFD[POLL_STDOUT].fd,
                    temOUTBuf + lto->rawOUTBytes, (SIZE_T) readOUTMax);

    if (nReadOUT < 0) {
#if defined(DEBUG_LTERM) && !defined(USE_NSPR_IO)
      int errcode = errno;
      /*    perror("ltermReceiveData"); */
#else
      int errcode = 0;
#endif
      LTERM_ERROR( "ltermReceiveData: Error %d in reading from process STDOUT\n",
                   errcode);
      return -1;
    }

    if (nReadOUT == 0) {
      /* End of file => pseudo-TTY has been closed */
      LTERM_LOG(ltermReceiveData,31,("pseudo-TTY has been closed\n"));

      /* Suspend LTERM */
      lts->suspended = 1;

      return -2;
    }

    LTERM_LOG(ltermReceiveData,32,("Read %d raw bytes from STDOUT\n", nReadOUT));

    nTotalOUT = nReadOUT + lto->rawOUTBytes;
  }

  n_decoded_tot = 0;

  if (lts->readERRfirst) {
    /* Decode STDERR data first */
    interleavedBytes = 0;
    n_decoded = ltermAppendOutput(lts, temERRBuf, nTotalERR,
                                  LTERM_STDERR_STYLE,
                                  lts->interleave, &interleavedBytes,
                                  MAXRAWINCOMPLETE,
                                  &(lto->rawERRBytes), lto->rawERRBuf);
    if (n_decoded < 0)
      return -1;
    n_decoded_tot += n_decoded;

    /* Decode STDOUT data next */
    n_decoded = ltermAppendOutput(lts, temOUTBuf, nTotalOUT,
                                  LTERM_STDOUT_STYLE,
                                  0, NULL,
                                  MAXRAWINCOMPLETE,
                                  &(lto->rawOUTBytes), lto->rawOUTBuf);
    if (n_decoded < 0)
      return -1;
    n_decoded_tot += n_decoded;

    if (interleavedBytes > 0) {
      /* Decode remaining STDERR data */
      n_decoded = ltermAppendOutput(lts, temERRBuf+interleavedBytes,
                                    nTotalERR-interleavedBytes,
                                    LTERM_STDERR_STYLE,
                                    0, NULL,
                                    MAXRAWINCOMPLETE,
                                    &(lto->rawERRBytes), lto->rawERRBuf);
      if (n_decoded < 0)
        return -1;
      n_decoded_tot += n_decoded;
    }

  } else {
    /* Decode STDOUT data first */
    interleavedBytes = 0;
    n_decoded = ltermAppendOutput(lts, temOUTBuf, nTotalOUT,
                                  LTERM_STDOUT_STYLE,
                                  lts->interleave, &interleavedBytes,
                                  MAXRAWINCOMPLETE,
                                  &(lto->rawOUTBytes), lto->rawOUTBuf);
    if (n_decoded < 0)
      return -1;
    n_decoded_tot += n_decoded;

    /* Decode STDERR data next */
    n_decoded = ltermAppendOutput(lts, temERRBuf, nTotalERR,
                                  LTERM_STDERR_STYLE,
                                  0, NULL,
                                  MAXRAWINCOMPLETE,
                                  &(lto->rawERRBytes), lto->rawERRBuf);
    if (n_decoded < 0)
      return -1;
    n_decoded_tot += n_decoded;

    if (interleavedBytes > 0) {
      /* Decode remaining STDOUT data */
      n_decoded = ltermAppendOutput(lts, temOUTBuf+interleavedBytes,
                                    nTotalOUT-interleavedBytes,
                                    LTERM_STDOUT_STYLE,
                                    0, NULL,
                                    MAXRAWINCOMPLETE,
                                    &(lto->rawOUTBytes), lto->rawOUTBuf);
      if (n_decoded < 0)
        return -1;
      n_decoded_tot += n_decoded;
    }
  }

  if (n_decoded_tot > 0) {
    /* New characters have been decoded; no longer incomplete escape seq. */
    lto->incompleteEscapeSequence = 0;
  }

  return n_decoded_tot;
}


/** Process COUNT bytes raw data from CBUF, converting complete
 * character encodings to Unicode, saving RAWINCOMPLETEBYTES in
 * RAWINCOMPLETEBUF, whose maximum size is specified by RAWINCOMPLETEMAX.
 * If INTERLEAVECHECK, check for starting NEWLINE and other interleavable
 * characters, and if found "decode" just those characters and set
 * INTERLEAVEDBYTES to the number of bytes consumed on return;
 * otherwise INTERLEAVEDBYTES is set to 0.
 * The decoded characters are appended to the decodedOutput buffer,
 * with style STYLE.
 * @return the number of characters appended (>=0) if successful,
 *         -1 if an error occurred during processing, or
 *         -2 if pseudo-TTY has been closed.
 */
static int ltermAppendOutput(struct lterms *lts, const char *cbuf, int count,
                             UNISTYLE style, int interleaveCheck,
                             int *interleavedBytes, int rawIncompleteMax,
                             int *rawIncompleteBytes, char *rawIncompleteBuf)
{
  struct LtermOutput *lto = &(lts->ltermOutput);
  int decodeMax, n_decoded, decodeNUL, j;

  LTERM_LOG(ltermAppendOutput,30,("count=%d, style=0x%X, iCheck=%d, rawIncMax=%d\n",
            count, style, interleaveCheck, rawIncompleteMax));

  if (interleaveCheck && (count > 0) && (cbuf[0] == '\n')) {
    /* Raw buffer starts with NEWLINE character; interleave */

    assert(lto->decodedChars < MAXCOLM1);

    /* "Decode" just the NEWLINE character */
    lto->decodedOutput[lto->decodedChars] = U_LINEFEED;
    lto->decodedStyle[lto->decodedChars] = LTERM_STDOUT_STYLE;

    lto->decodedChars++;

    *interleavedBytes = 1;

    LTERM_LOG(ltermAppendOutput,32,("INTERLEAVED %d bytes\n",
                                    *interleavedBytes));

    return 1;
  }

  if (interleavedBytes != NULL)
    *interleavedBytes = 0;

  if (count == 0)
    return 0;

  /* Decode all complete encoded character sequences */
  decodeMax = MAXCOLM1 - lto->decodedChars;
  decodeNUL = (lts->options & LTERM_NONUL_FLAG) == 0;
  n_decoded = ltermDecode(cbuf, count,
                          lto->decodedOutput+lto->decodedChars,
                          decodeMax, decodeNUL,
                          rawIncompleteBytes);

  if (n_decoded < 0)
    return -1;

  /* Save any incomplete raw byte encodings */
  if (*rawIncompleteBytes > rawIncompleteMax) {
    LTERM_ERROR("ltermAppendOutput: Error - too many incomplete raw characters\n");
    return -1;
  }

  for (j=0; j<*rawIncompleteBytes; j++)
    rawIncompleteBuf[j] = cbuf[j+count-*rawIncompleteBytes];

  /* Set decoded character styles */
  for (j=lto->decodedChars; j<lto->decodedChars+n_decoded; j++)
    lto->decodedStyle[j] = style;

  /* Increment decoded character count */
  lto->decodedChars += n_decoded;

  LTERM_LOG(ltermAppendOutput,32,("Appended %d bytes\n", n_decoded));

  return n_decoded;
}


/** Decodes N_TOTAL bytes in RAWBUF, returning upto DECODEMAX Unicode
 * characters in DECODEDBUF, leaving *RAWINCOMPLETEBYTES at the end of
 * RAWBUF undecoded.
 * Any NUL (zero) characters in RAWBUF are skipped, unless
 * decodeNUL is true.
 * If there is not enough space in DECODEDBUF for all characters to be
 * decoded, an error is returned.
 * @return the number of Unicoded characters decoded (>=0) or -1 on error.
 */
static int ltermDecode(const char *rawBuf, int n_total,
                       UNICHAR* decodedBuf, int decodeMax,
                       int decodeNUL,
                       int *rawIncompleteBytes)
{
  int n_decoded, result;

  LTERM_LOG(ltermDecode,40,("\n"));

  if (decodeMax < n_total) {
    LTERM_ERROR("ltermDecode: Error - decode buffer overflow\n");
    return -1;
  }

  result = utf8toucs(rawBuf, n_total, decodedBuf, decodeMax,
                     !decodeNUL, rawIncompleteBytes, &n_decoded);

  if (result != 0) {
    LTERM_WARNING("ltermDecode: Warning - Invalid UTF8 data encountered\n");
  }

  LTERM_LOG(ltermDecode,41,("result=%d, incomplete=%d, n_decoded=%d\n",
                            result, *rawIncompleteBytes, n_decoded));
  LTERM_LOGUNICODE(ltermDecode,42,(decodedBuf, n_decoded));

  return n_decoded;
}


/** Search for prompt string in the output line buffer.
 * TEMPORARY implementation: assume promptRegexp is just a list of delimiters
 * @return the length of the matching prompt string, or
 *         0 if there is no match.
 */
static int ltermPromptLocate(struct lterms *lts)
{
  struct LtermOutput *lto = &(lts->ltermOutput);
  int prefixCount, promptLen;

  LTERM_LOG(ltermPromptLocate,49,("lto->outputChars=%d\n",
                lto->outputChars));

  /* Assert that there is at least one free character in the buffer */
  assert(lto->outputChars < MAXCOL);

  if (lto->outputChars == 0)
    return 0;

  /* Insert null character at the end of the output buffer */
  lto->outputLine[lto->outputChars] = U_NUL;

  /* Determine length of initial non-delimiter prefix */
  prefixCount = ucscspn(lto->outputLine, lts->promptRegexp);

  if (prefixCount+1 >= lto->outputChars) {
    promptLen = 0;

  } else {
    /* Skip any whitespace following the delimiter */
    const UNICHAR spaceStr[3] = {U_SPACE, U_TAB, U_NUL};
    int spaceCount = ucsspn(lto->outputLine+prefixCount+1, spaceStr);

    promptLen = prefixCount + 1 + spaceCount;

    LTERM_LOGUNICODE(ltermPromptLocate,41,(lto->outputLine, promptLen));
  }

  return promptLen;
}
