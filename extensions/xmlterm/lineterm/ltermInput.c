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

/* ltermInput.c: LTERM PTY input data processing
 */

/* public declarations */
#include "lineterm.h"

/* private declarations */
#include "ltermPrivate.h"


static int ltermLineInput(struct lterms *lts, const UNICHAR *buf, int count,
                          int *opcodes);
static int ltermMetaInput(struct lterms *lts);
static int ltermRequestCompletion(struct lterms *lts, UNICHAR uch);


/** Processes plain text input data and returns
 * OPCODES ::= LINEDATA  (  INPUT ( NEWLINE HIDE? )?
 *                        | INPUT META ( COMPLETION | NEWLINE HIDE? ) )
 * if echoable input data was processed.
 * (OPCODES is set to zero if raw input data was processed)
 * Called from ltermWrite
 * @return 0 on success,
 *        -1 on error, and
 *        -2 if pseudo-TTY has been closed.
 */
int ltermPlainTextInput(struct lterms *lts,
                        const UNICHAR *buf, int count, int *opcodes)
{
  struct LtermInput *lti = &(lts->ltermInput);
  int returnCode;

  LTERM_LOG(ltermPlainTextInput,20,
    ("count=%d, lti->inputMode=%d\n", count, lti->inputMode));

  if (lti->inputMode == LTERM0_RAW_MODE) {
    /* Transmit characters immediately to child process; no buffering */

    LTERM_LOG(ltermPlainTextInput,29,
                                    ("Raw mode, transmitting %d characters\n",
                                     count));

    if (ltermSendData(lts, buf, count) != 0)
      return -1;

    *opcodes = 0;

  } else {
    /* Not raw input mode; process line mode input */
    int processTrailingTab = 0;

    LTERM_LOG(ltermPlainTextInput,21,
              ("Line mode, lts->commandNumber=%d, inputMode=%d\n",
               lts->commandNumber, lti->inputMode));

    if ((lti->inputMode >= LTERM3_COMPLETION_MODE) &&
        (lts->commandNumber == 0)) {
      /* Downgrade input mode */
      lti->inputMode = LTERM2_EDIT_MODE;

      LTERM_LOG(ltermPlainTextInput,21,
                ("------------ Downgraded input mode=%d\n\n",
                 lti->inputMode));

    } else  if ((lti->inputMode < lts->maxInputMode) &&
                (lts->commandNumber != 0)) {
      /* Upgrade input mode */
      int priorInputMode = lti->inputMode;

      /* Set input mode (possibly allowing completion) */
      lti->inputMode = lts->maxInputMode;

      /* Do not allow command completion without TTY echo */
      if ( (lts->disabledInputEcho || lts->noTTYEcho) &&
           (lti->inputMode > LTERM2_EDIT_MODE) )
        lti->inputMode = LTERM2_EDIT_MODE;

      if ((lti->inputChars > 0) &&
          (priorInputMode < LTERM3_COMPLETION_MODE) &&
          (lti->inputMode >= LTERM3_COMPLETION_MODE)) {
        /* Process prior input TABs before switching to completion mode */
        int j;

        if ((count == 0) &&
            (lti->inputCursorGlyph == lti->inputGlyphs) &&
            (lti->inputGlyphColIndex[lti->inputGlyphs] == lti->inputCols) &&
            (lti->inputColCharIndex[lti->inputCols] == lti->inputChars) &&
            (lti->inputLine[lti->inputChars] == U_TAB)) {
          /* Trailing TAB in prior input; delete it, and process it later */
          if (ltermDeleteGlyphs(lti, 1) != 0)
              return -1;
          processTrailingTab = 1;
        }

        /* Replace all input TABs with spaces */
        for (j=0; j < lti->inputChars; j++) {
          if (lti->inputLine[j] == U_TAB)
            lti->inputLine[j] = U_SPACE;
        }
      }

      LTERM_LOG(ltermPlainTextInput,21,
                ("------------ Upgraded input mode=%d, trailingTab=%d\n\n",
                 lti->inputMode, processTrailingTab));

    }

    if (processTrailingTab) {
      /* Re-process trailing TAB */
      UNICHAR uch = U_TAB;

      assert(count == 0);

      LTERM_LOG(ltermPlainTextInput,21,("Reprocessing trailing TAB\n"));

      returnCode= ltermLineInput(lts, &uch, 1, opcodes);
      if (returnCode < 0)
        return returnCode;

    } else {
      /* Process new input characters */
      returnCode = ltermLineInput(lts, buf, count, opcodes) != 0;
      if (returnCode < 0)
        return returnCode;
    }
  }

  return 0;
}


/** Cancels a prior command line completion request
 * @return  0 on success,
 *         -1 on error
 */
int ltermCancelCompletion(struct lterms *lts)
{

  LTERM_LOG(ltermCancelCompletion,40,
            ("++++++++++++ CANCELED COMPLETION REQUEST\n\n"));

  if (lts->completionRequest != LTERM_NO_COMPLETION) {

    /* Kill input line transmitted to process */
    if (ltermSendData(lts, lts->control+TTYKILL, 1) != 0)
      return -1;

    lts->completionRequest = LTERM_NO_COMPLETION;
  }

  return 0;
}


/** Inserts plain text character UCH as a single-column glyph at the
 * current input cursor location, translating to escape sequence if needed.
 * @return 0 on success, -1 on error.
 */
int ltermInsertChar(struct LtermInput *lti, UNICHAR uch)
{
  UNICHAR* escapeSequence;
  int insChars, insertColIndex, insertCharIndex, j;

  LTERM_LOG(ltermInsertChar,40,("inserting character 0x%x at glyph %d\n",
                                   uch, lti->inputCursorGlyph));

  /* Ignore null character */
  if (uch == 0)
    return 0;

  escapeSequence = NULL;
  insChars = 1;

#if 0
  /* COMMENTED OUT: Plain text not escaped; use code later in HTML insert */
  /* Check if plain text character needs to be escaped for XML/HTML */
  for (j=0; j<LTERM_PLAIN_ESCAPES; j++) {

    if (uch == ltermGlobal.escapeChars[j]) {
      /* Insert escape sequence rather than character */
      escapeSequence = ltermGlobal.escapeSeq[j];
      insChars =       ltermGlobal.escapeLen[j];
      LTERM_LOG(ltermInsertChar,42,("escape index=%d\n", j));
      break;
    }
  }
#endif /* 0 */

  if (lti->inputChars+insChars > MAXCOLM1) {
    /* Input buffer overflow; ignore insert character */
    LTERM_WARNING("ltermInsertChar: Warning - input line buffer overflow\n");
    return 0;
  }

  assert(lti->inputChars >= 0);
  assert(lti->inputCols >= 0);
  assert(lti->inputGlyphs >= 0);
  assert(lti->inputCursorGlyph >= 0);

  assert(lti->inputCols <= lti->inputChars);
  assert(lti->inputGlyphs <= lti->inputCols);

  insertColIndex = lti->inputGlyphColIndex[lti->inputCursorGlyph];
  insertCharIndex = lti->inputColCharIndex[insertColIndex];

  LTERM_LOG(ltermInsertChar,41,("insertColIndex=%d, insertCharIndex=%d, insChars=%d\n",
           insertColIndex, insertCharIndex, insChars));

  /* Shift portion of input line to the right;
     remember that the column/glyph index arrays have an extra element */
  for (j=lti->inputChars - 1; j >= insertCharIndex; j--)
    lti->inputLine[j+insChars] = lti->inputLine[j];

  for (j=lti->inputCols; j >= insertColIndex; j--)
    lti->inputColCharIndex[j+1] = lti->inputColCharIndex[j]+insChars;

  for (j=lti->inputGlyphs; j >= lti->inputCursorGlyph; j--) {
    lti->inputGlyphCharIndex[j+1] = lti->inputGlyphCharIndex[j]+insChars;
    lti->inputGlyphColIndex[j+1] = lti->inputGlyphColIndex[j]+1;
  }

  /* Insert character(s) in input line */
  if (escapeSequence == NULL) {
    lti->inputLine[insertCharIndex] = uch;
  } else {
    for (j=0; j < insChars; j++)
      lti->inputLine[j+insertCharIndex] = escapeSequence[j];
  }

  /* Insert column/glyph */
  lti->inputColCharIndex[insertColIndex] = insertCharIndex;

  lti->inputGlyphCharIndex[lti->inputCursorGlyph] = insertCharIndex;
  lti->inputGlyphColIndex[lti->inputCursorGlyph] = insertColIndex;

  lti->inputChars += insChars;       /* Increment character count */

  lti->inputCols++;                  /* Increment column count */

  lti->inputGlyphs++;                /* Increment glyph count */

  lti->inputCursorGlyph++;           /* Reposition cursor */

  return 0;
}


/** switches to raw input mode */
void ltermSwitchToRawMode(struct lterms *lts)
{
  struct LtermInput *lti = &(lts->ltermInput);

  LTERM_LOG(ltermSwitchToRawMode,40,("\n"));

  if (lti->inputMode != LTERM0_RAW_MODE) {
    /* Do other things ... */
    lti->inputMode = LTERM0_RAW_MODE;
  }
}


/** clears input line buffer and switches to regular input mode */
void ltermClearInputLine(struct lterms *lts)
{
  struct LtermInput *lti = &(lts->ltermInput);

  LTERM_LOG(ltermClearInputLine,40,("\n"));

  lti->inputChars = 0;

  lti->inputCols = 0;
  lti->inputColCharIndex[0] = 0;

  lti->inputGlyphs = 0;
  lti->inputGlyphCharIndex[0] = 0;
  lti->inputGlyphColIndex[0] = 0;

  lti->inputCursorGlyph = 0;

  if (lts->maxInputMode >= LTERM2_EDIT_MODE)
    lti->inputMode = LTERM2_EDIT_MODE;
  else
    lti->inputMode = lts->maxInputMode;

  lti->escapeFlag = 0;
  lti->escapeCSIFlag = 0;
  lti->escapeCSIArg = 0;
}


/** Processes an input string in canonical or higher mode and returns
 * OPCODES ::= LINEDATA  (  INPUT (NEWLINE HIDE?)?
 *                        | INPUT META (COMPLETION|NEWLINE HIDE?) )
 * if echoable input data was processed.
 * (OPCODES is set to zero if raw input data was processed)
 * Called from ltermPlainTextInput
 * @return 0 on success,
 *        -1 on error, and
 *        -2 if pseudo-TTY has been closed.
 */
static int ltermLineInput(struct lterms *lts,
                          const UNICHAR *buf, int count, int *opcodes)
{
  struct LtermInput *lti = &(lts->ltermInput);
  UNICHAR uch;
  int charIndex, metaInput;

  /* Default returned opcodes (maybe overridden) */
  *opcodes = LTERM_LINEDATA_CODE | LTERM_INPUT_CODE;

  charIndex = 0;

  LTERM_LOG(ltermLineInput,30,
            ("count=%d, lti->inputMode=%d, inputCursorGlyph=%d\n",
             count, lti->inputMode, lti->inputCursorGlyph));
  LTERM_LOGUNICODE(ltermLineInput,31,(buf, count));
  LTERM_LOG(ltermLineInput,31,("Glyphs=%d,Cols=%d,Chars=%d\n",
                            lti->inputGlyphs, lti->inputCols, lti->inputChars));

  while (charIndex < count) {
    uch = buf[charIndex];

    if (uch == U_ESCAPE) {
      /* Escape */
      lti->escapeFlag = 1;
      uch = U_NUL;

    } else if (lti->escapeFlag) {
      /* Escaped character */
      lti->escapeFlag = 0;

      switch (uch) {

      case U_LBRACKET:
        /* Start of escape code sequence */
        lti->escapeCSIFlag = 1;
        lti->escapeCSIArg = 0;
        uch = U_NUL;
        break;

      default:
        uch = U_NUL;
      }

    } else if (lti->escapeCSIFlag) {
      /* Character part of escape code sequence */

      LTERM_LOG(ltermLineInput,38,("Escape code sequence - 0x%x\n", uch));

      if ((uch >= (UNICHAR)U_ZERO && uch <= (UNICHAR)U_NINE)) {
        /* Process numerical argument to escape code sequence */
        lti->escapeCSIArg = lti->escapeCSIArg*10 + (uch - U_ZERO);
        uch = U_NUL;

      } else {
        /* End of escape code sequence */
        lti->escapeCSIFlag = 0;

        /* NOTE: Input CSI escape sequence; may not be portable */

        /* SUN arrow key bindings */
        switch (uch) {
        case U_A_CHAR:
          uch = U_CTL_P;
          break;
        case U_B_CHAR:
          uch = U_CTL_N;
          break;
        case U_C_CHAR:
          uch = U_CTL_F;
          break;
        case U_D_CHAR:
          uch = U_CTL_B;
          break;
        default:
          uch = U_NUL;
        }
      }
    }

    if ( ((uch >= (UNICHAR)U_SPACE) && (uch != (UNICHAR)U_DEL)) ||
         ((uch == (UNICHAR)U_TAB) && (lti->inputMode <= LTERM2_EDIT_MODE)) ) {
      /* printable character or non-completion mode TAB; insert in buffer */
      /* (NEED TO UPDATE THIS CHECK FOR UNICODE PRINTABILITY) */

      LTERM_LOG(ltermLineInput,39,("inserting printable character - %c\n",
                   (char) uch));

      /* Insert character */
      if (ltermInsertChar(lti, uch) != 0)
        return -1;

    } else {
      /* Control character */

      /* Translate carriage returns to linefeeds in line mode
         (***NOTE*** may not be portable out of *nix) */
      if (uch == U_CRETURN)
        uch = U_LINEFEED;

      /* Line break control characters */
      if ( (uch == U_LINEFEED)               ||
           (uch == lts->control[TTYDISCARD]) ||
           (uch == lts->control[TTYSUSPEND]) ||
           (uch == lts->control[TTYINTERRUPT])) {
        /* Newline/TTYdiscard/TTYsuspend/TTYinterrupt character */

        /* Assert that linebreak character occurs at end of buffer;
         * enforced by lterm_write.
         */
        assert(charIndex == count-1);

        /* Check if meta input line */
        metaInput = ltermMetaInput(lts);

        if ((uch == lts->control[TTYDISCARD]) && !metaInput
            && (lts->commandNumber == 0)) {
          /* Not meta/command line; simply transmit discard character */
          if (ltermSendData(lts, lts->control+TTYDISCARD, 1) != 0)
            return -1;

        } else {
          /* Newline behaviour, with hide option */
          LTERM_LOG(ltermLineInput,31,("------------ NEWLINE (0x%x)\n\n",
                                         uch));
          LTERM_LOGUNICODE(ltermLineInput,31,( lti->inputLine,
                                                 lti->inputChars));

          /* The NEWLINE code tells ltermReturnInputLine to clear
           * the input line buffer after copying it
           */
          *opcodes = LTERM_LINEDATA_CODE | LTERM_INPUT_CODE
                                         | LTERM_NEWLINE_CODE;

          if (uch == lts->control[TTYDISCARD]) {
            *opcodes |= LTERM_HIDE_CODE;
            uch = U_LINEFEED;   /* essentially newline behaviour otherwise */
          }

          if (uch == lts->control[TTYINTERRUPT]) {
            /* Interrupt output operations, if necessary */
            if (ltermInterruptOutput(lts) != 0)
              return -1;
          }

          if (metaInput) {
            /* meta input; do not send line */
            *opcodes |= LTERM_META_CODE;
          } else {
            /* Send line and copy to echo buffer */
            if (ltermSendLine(lts, uch, (uch != U_LINEFEED),
                              LTERM_NO_COMPLETION) != 0)
              return -1;
          }

        }

      } else if (uch == lts->control[TTYKILL]) {
        /* kill line */
        ltermClearInputLine(lts);

        LTERM_LOG(ltermLineInput,31,("TTYKILL\n"));

      } else if ((uch == U_BACKSPACE) || (uch == U_DEL) ||
                 (uch == lts->control[TTYERASE])) {
        /* erase glyph */
        if (ltermDeleteGlyphs(lti, 1) != 0)
          return -1;

        LTERM_LOG(ltermLineInput,39,("TTYERASE=0x%x/0x%x\n",
                                     lts->control[TTYERASE], uch ));

      } else {
        /* other control characters */

        LTERM_LOG(ltermLineInput,32,("^%c\n", uch+U_ATSIGN));

        if (lti->inputMode < LTERM2_EDIT_MODE) {
          /* Non-edit mode; simply transmit control character */
          if (ltermSendData(lts, &uch, 1) != 0)
            return -1;

        } else {
          /* Edit input mode */

          if (uch == U_CTL_D) {
            /* Special handling for ^D */

            if (lti->inputChars == 0) {
              /* Lone ^D in input line, simply transmit it */
              if (ltermSendData(lts, &uch, 1) != 0)
                return -1;
              uch = U_NUL;

            } else if (lti->inputCursorGlyph < lti->inputGlyphs) {
              /* Cursor not at end of line; delete to right */
              if (ltermDeleteGlyphs(lti, -1) != 0)
                return -1;
              uch = U_NUL;
            }
          }
              
          switch (uch) {

          case U_NUL:                /* Null character; ignore */
            break;

          case U_CTL_B:              /* move cursor backward */
            if (lti->inputCursorGlyph > 0) {
              lti->inputCursorGlyph--;
            }
            break;

          case U_CTL_F:              /* move cursor forward */
            if (lti->inputCursorGlyph < lti->inputGlyphs) {
              lti->inputCursorGlyph++;
            }
            break;

          case U_CTL_A:              /* position cursor at beginning of line */
            lti->inputCursorGlyph = 0;
            break;

          case U_CTL_E:              /* position cursor at end of line */
            lti->inputCursorGlyph = lti->inputGlyphs;
            break;

          case U_CTL_K:              /* delete to end of line */
            if (ltermDeleteGlyphs(lti,-(lti->inputGlyphs-lti->inputCursorGlyph))
                != 0)
              return -1;
            break;

          case U_CTL_L:              /* form feed */
          case U_CTL_R:              /* redisplay */
            break;

          case U_CTL_D:              /* ^D at end of non-null input line */
          case U_CTL_N:              /* dowN history */
          case U_CTL_P:              /* uP history */
          case U_CTL_Y:              /* yank */
          case U_TAB:                /* command completion */

            /* Assert that completion character occurs at end of buffer;
             * enforced by lterm_write.
             */
            assert(charIndex == count-1);

            metaInput = ltermMetaInput(lts);

            if (metaInput) {
              /* Meta input command completion */
              LTERM_LOG(ltermLineInput,40,
                        ("++++++++++++ meta COMPLETION uch=0x%X\n\n", uch));
              if (uch == U_TAB) {
                *opcodes = LTERM_LINEDATA_CODE | LTERM_INPUT_CODE
                                               | LTERM_META_CODE
                                               | LTERM_COMPLETION_CODE;
              } else {
                LTERM_WARNING("ltermLineInput: Warning - meta command completion not yet implemented for uch=0x%x\n", uch);
              }

            } else if (lti->inputMode >= LTERM3_COMPLETION_MODE) {
              /* Completion mode; non-completion TABs already processed */

              if (ltermRequestCompletion(lts, uch) != 0)
                return -1;
            }

            break;

          default:              /* Transmit any other control character */
            if (ltermSendData(lts, &uch, 1) != 0)
              return -1;
          }
        }
      }
    }

    /* Increment character index */
    charIndex++;
  }

  return 0;
}


/** Check if input line contains a meta delimiter;
 * @return 1 if it does, 0 otherwise.
 */
static int ltermMetaInput(struct lterms *lts)
{
  struct LtermInput *lti = &(lts->ltermInput);
  UNICHAR *delimLoc, *ustr, *ustr2;

  LTERM_LOG(ltermMetaInput,40,("\n"));

  if (lts->options & LTERM_NOMETA_FLAG)
    return 0;

  /* Assert that there is at least one free character position in the buffer */
  assert(lti->inputChars < MAXCOL);

  /* Insert null character at the end of the input buffer */
  lti->inputLine[lti->inputChars] = U_NUL;

  /* Locate first occurrence of meta delimiter in input line */
  delimLoc = ucschr(lti->inputLine, ltermGlobal.metaDelimiter);

  if (delimLoc == NULL)
    return 0;

  for (ustr=lti->inputLine; ustr<delimLoc; ustr++)  /* skip spaces/TABs */
    if ((*ustr != U_SPACE) && (*ustr != U_TAB)) break;

  if (ustr == delimLoc) {
    /* Nameless meta command */
    LTERM_LOG(ltermMetaInput,41,("Nameless meta command\n"));
    return 1;
  }

  if (!IS_ASCII_LETTER(*ustr)) /* meta command must start with a letter */
    return 0;

  for (ustr2=ustr+1; ustr2<delimLoc; ustr2++)
    if (!IS_ASCII_LETTER(*ustr2) && !IS_ASCII_DIGIT(*ustr2))
      return 0;

  LTERM_LOG(ltermMetaInput,41,("Named meta command\n"));

  return 1;
}


/** Requests command line completion from process corresponding to
 * control character UCH.
 * @return  0 on success,
 *         -1 on error
 */
static int ltermRequestCompletion(struct lterms *lts, UNICHAR uch)
{
  LTERM_LOG(ltermRequestCompletion,40,
            ("++++++++++++ COMPLETION REQUEST uch=0x%X\n\n", uch));

  switch (uch) {
  case U_TAB:
    /* Send line and copy to echo buffer */
    if (ltermSendLine(lts, uch, 0, LTERM_TAB_COMPLETION) != 0)
      return -1;
    break;
  case U_CTL_P:
  case U_CTL_N:
    /* Send line and copy to echo buffer */
    if (ltermSendLine(lts, uch, 0, LTERM_HISTORY_COMPLETION) != 0)
      return -1;
    break;
  default:
    LTERM_WARNING("ltermCompletionRequest: Warning - command completion not yet implemented for uch=0x%x\n", uch);
  }

  return 0;
}


/** Deletes glyphs from the input line.
 * If COUNT > 0, glyphs are deleted to the left of the cursor.
 * If COUNT < 0, glyphs are deleted to the right of the cursor.
 * Called from ltermLineInput.
 * @return 0 on success, -1 on error.
 */
int ltermDeleteGlyphs(struct LtermInput *lti, int count)
{
  int leftGlyph, leftColIndex, leftCharIndex;
  int rightGlyph, rightColIndex, rightCharIndex;
  int deleteGlyphs, deleteCols, deleteChars, j;

  LTERM_LOG(ltermDeleteGlyphs,40,("deleting %d glyphs from glyph %d\n",
            count, lti->inputCursorGlyph));

  if (count >= 0) {
    /* Delete to the left */
    deleteGlyphs = count;

    /* Limit the number of glyphs deleted to that present to the left */
    if (deleteGlyphs > lti->inputCursorGlyph)
      deleteGlyphs = lti->inputCursorGlyph;

    rightGlyph = lti->inputCursorGlyph;
    leftGlyph = rightGlyph - deleteGlyphs;

  } else {
    /* Delete to the right */
    deleteGlyphs = -count;

    /* Limit the number of glyphs deleted to that present to the right */
    if (deleteGlyphs > (lti->inputGlyphs - lti->inputCursorGlyph))
      deleteGlyphs = lti->inputGlyphs - lti->inputCursorGlyph;

    leftGlyph = lti->inputCursorGlyph;
    rightGlyph = leftGlyph + deleteGlyphs;
  }

  leftColIndex = lti->inputGlyphColIndex[leftGlyph];
  leftCharIndex = lti->inputGlyphCharIndex[leftGlyph];

  rightColIndex = lti->inputGlyphColIndex[rightGlyph];
  rightCharIndex = lti->inputGlyphCharIndex[rightGlyph];

  deleteCols =  rightColIndex  - leftColIndex;
  deleteChars = rightCharIndex - leftCharIndex;

  LTERM_LOG(ltermDeleteGlyphs,41,("deleteCols=%d, deleteChars=%d\n",
                                    deleteCols, deleteChars));

  LTERM_LOG(ltermDeleteGlyphs,42,("leftGlyph=%d, leftCol=%d, leftChar=%d\n",
                                    leftGlyph, leftColIndex, leftCharIndex));

  LTERM_LOG(ltermDeleteGlyphs,42,("rightGlyph=%d, rightCol=%d, rightChar=%d\n",
                                    rightGlyph, rightColIndex, rightCharIndex));

  /* Shift portion of input line to the left;
     remember that the column/glyph index arrays have an extra element */
  for (j = leftCharIndex; j < lti->inputChars-deleteChars; j++)
    lti->inputLine[j] = lti->inputLine[j+deleteChars];

  for (j = leftColIndex; j <= lti->inputCols-deleteCols; j++)
    lti->inputColCharIndex[j] =  lti->inputColCharIndex[j+deleteCols]
                               - deleteChars;

  for (j = leftGlyph; j <= lti->inputGlyphs-deleteGlyphs; j++)
    lti->inputGlyphColIndex[j] =  lti->inputGlyphColIndex[j+deleteGlyphs]
                                - deleteCols;

  lti->inputChars -= deleteChars;             /* Decrement character count */

  lti->inputCols -= deleteCols;               /* Decrement column count */

  lti->inputGlyphs -= deleteGlyphs;           /* Decrement glyph count */

  if (count > 0)
    lti->inputCursorGlyph -= deleteGlyphs;    /* Reposition glyph cursor */

  return 0;
}


/** Transmits COUNT Unicode characters from BUF to child process
 * after translating Unicode to UTF8 or Latin1, as appropriate.
 * The data is transmitted in smallish chunks so as not to overflow the
 * PTY input buffer.
 * @return 0 on successful write, -1 on error.
 */
int ltermSendData(struct lterms *lts, const UNICHAR *buf, int count)
{
  char ch, ptyBuf[MAXPTYIN];
  int remainingChars, chunkSize, success;

  assert(lts != NULL);
  assert(count >= 0);

  LTERM_LOG(ltermSendData,40,("count=%d\n", count));
  LTERM_LOGUNICODE(ltermSendData,41,(buf, count));

  if ((count == 1) && (*buf < 0x80)) { 
    /* Optimized code to transmit single ASCII character */
    ch = (char) *buf;

    if (lts->ptyMode)
#ifndef USE_NSPR_IO
      success = (write(lts->pty.ptyFD, &ch, 1) == 1);
#else
      assert(0);
#endif
    else
      success = (WRITE(lts->ltermProcess.processIN, &ch, 1) == 1);

    if (!success) {
#if defined(DEBUG_LTERM) && !defined(USE_NSPR_IO)
      int errcode = errno;
      perror("ltermSendData");
#else
      int errcode = 0;
#endif
      LTERM_ERROR("ltermSendData: Error %d in writing to child STDIN\n",
                  errcode);
      return -1;
    }

    return 0;
  }

  remainingChars = count;
  while (remainingChars > 0) {
    /* Convert Unicode to UTF8 */
    ucstoutf8(&buf[count-remainingChars], remainingChars,
              ptyBuf, MAXPTYIN,
              &remainingChars, &chunkSize);

    assert(chunkSize > 0);

    LTERM_LOG(ltermSendData,42,("remainingChars=%d, chunkSize=%d\n",
                                 remainingChars, chunkSize));

    /* Send UTF8 to process */
    if (ltermSendChar(lts, ptyBuf, chunkSize) != 0)
      return -1;
  }

  return 0;
}


/** Transmits COUNT characters from BUF to child process.
 * @return 0 on successful write, -1 on error.
 */
int ltermSendChar(struct lterms *lts, const char *buf, int count)
{
  int success;

  LTERM_LOG(ltermSendChar,50,("count=%d\n", count));

  if (lts->ptyMode)
#ifndef USE_NSPR_IO
    success = (write(lts->pty.ptyFD, buf,
                     (SIZE_T) count) == count);
#else
  assert(0);
#endif
  else
    success = (WRITE(lts->ltermProcess.processIN, buf,
                     (SIZE_T) count) == count);

  if (!success) {
#if defined(DEBUG_LTERM) && !defined(USE_NSPR_IO)
    int errcode = errno;
    perror("ltermSendChar");
#else
    int errcode = 0;
#endif
    LTERM_ERROR("ltermSendChar: Error %d in writing to child STDIN\n",
                errcode);
    return -1;
  }

  return 0;
}
