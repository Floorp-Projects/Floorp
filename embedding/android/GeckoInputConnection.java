/* -*- Mode: Java; tab-width: 20; indent-tabs-mode: nil; -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Android code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Michael Wu <mwu@mozilla.com>
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

package org.mozilla.gecko;

import java.io.*;
import java.util.*;
import java.util.concurrent.*;
import java.util.concurrent.atomic.*;

import android.os.*;
import android.app.*;
import android.text.*;
import android.text.style.*;
import android.view.*;
import android.view.inputmethod.*;
import android.content.*;
import android.R;

import android.util.*;

public class GeckoInputConnection
    extends BaseInputConnection
    implements TextWatcher
{
    public GeckoInputConnection (View targetView) {
        super(targetView, true);
        mQueryResult = new SynchronousQueue<String>();
    }

    @Override
    public boolean beginBatchEdit() {
        //Log.d("GeckoAppJava", "IME: beginBatchEdit");

        return true;
    }

    @Override
    public boolean commitCompletion(CompletionInfo text) {
        //Log.d("GeckoAppJava", "IME: commitCompletion");

        return commitText(text.getText(), 1);
    }

    @Override
    public boolean commitText(CharSequence text, int newCursorPosition) {
        //Log.d("GeckoAppJava", "IME: commitText");

        setComposingText(text, newCursorPosition);
        finishComposingText();

        return true;
    }

    @Override
    public boolean deleteSurroundingText(int leftLength, int rightLength) {
        //Log.d("GeckoAppJava", "IME: deleteSurroundingText");

        /* deleteSurroundingText is supposed to ignore the composing text,
            so we cancel any pending composition, delete the text, and then
            restart the composition */

        if (mComposing) {
            // Cancel current composition
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(0, 0, 0, 0, 0, 0, null));
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_COMPOSITION_END, 0, 0));
        }

        // Select text to be deleted
        int delStart, delLen;
        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_GET_SELECTION, 0, 0));
        try {
            mQueryResult.take();
        } catch (InterruptedException e) {
            Log.e("GeckoAppJava", "IME: deleteSurroundingText interrupted");
            return false;
        }
        delStart = mSelectionStart > leftLength ?
                    mSelectionStart - leftLength : 0;
        delLen = mSelectionStart + rightLength - delStart;
        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_SET_SELECTION, delStart, delLen));

        // Restore composition / delete text
        if (mComposing) {
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_COMPOSITION_BEGIN, 0, 0));
            if (mComposingText.length() > 0) {
                /* IME_SET_TEXT doesn't work well with empty strings */
                GeckoAppShell.sendEventToGecko(
                    new GeckoEvent(0, mComposingText.length(),
                                   GeckoEvent.IME_RANGE_RAWINPUT,
                                   GeckoEvent.IME_RANGE_UNDERLINE, 0, 0,
                                   mComposingText.toString()));
            }
        } else {
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_DELETE_TEXT, 0, 0));
        }
        return true;
    }

    @Override
    public boolean endBatchEdit() {
        //Log.d("GeckoAppJava", "IME: endBatchEdit");

        return true;
    }

    @Override
    public boolean finishComposingText() {
        //Log.d("GeckoAppJava", "IME: finishComposingText");

        if (mComposing) {
            // Set style to none
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(0, mComposingText.length(),
                               GeckoEvent.IME_RANGE_RAWINPUT, 0, 0, 0,
                               mComposingText));
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_COMPOSITION_END, 0, 0));
            mComposing = false;
            mComposingText = null;

            // Make sure caret stays at the same position
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_SET_SELECTION,
                               mCompositionStart + mCompositionSelStart, 0));
        }
        return true;
    }

    @Override
    public int getCursorCapsMode(int reqModes) {
        //Log.d("GeckoAppJava", "IME: getCursorCapsMode");

        return 0;
    }

    @Override
    public Editable getEditable() {
        Log.w("GeckoAppJava", "IME: getEditable called from " +
            Thread.currentThread().getStackTrace()[0].toString());

        return null;
    }

    @Override
    public boolean performContextMenuAction(int id) {
        //Log.d("GeckoAppJava", "IME: performContextMenuAction");

        // First we need to ask Gecko to tell us the full contents of the
        // text field we're about to operate on.
        String text;
        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_GET_TEXT, 0, Integer.MAX_VALUE));
        try {
            text = mQueryResult.take();
        } catch (InterruptedException e) {
            Log.e("GeckoAppJava", "IME: performContextMenuAction interrupted");
            return false;
        }

        switch (id) {
            case R.id.selectAll:
                setSelection(0, mUpdateExtract.text.length());
                break;
            case R.id.cut:
                // Fill the clipboard
                GeckoAppShell.setClipboardText(text);
                // If GET_TEXT returned an empty selection, we'll select everything
                if (mSelectionLength <= 0)
                    GeckoAppShell.sendEventToGecko(
                        new GeckoEvent(GeckoEvent.IME_SET_SELECTION, 0, text.length()));
                GeckoAppShell.sendEventToGecko(
                    new GeckoEvent(GeckoEvent.IME_DELETE_TEXT, 0, 0));
                break;
            case R.id.paste:
                commitText(GeckoAppShell.getClipboardText(), 1);
                break;
            case R.id.copy:
                // If there is no selection set, we must be doing "Copy All",
                // otherwise, we need to get the selection from Gecko
                if (mSelectionLength > 0) {
                    GeckoAppShell.sendEventToGecko(
                        new GeckoEvent(GeckoEvent.IME_GET_SELECTION, 0, 0));
                    try {
                        text = mQueryResult.take();
                    } catch (InterruptedException e) {
                        Log.e("GeckoAppJava", "IME: performContextMenuAction interrupted");
                        return false;
                    }
                }
                GeckoAppShell.setClipboardText(text);
                break;
        }
        return true;
    }

    @Override
    public ExtractedText getExtractedText(ExtractedTextRequest req, int flags) {
        if (req == null)
            return null;

        //Log.d("GeckoAppJava", "IME: getExtractedText");

        ExtractedText extract = new ExtractedText();
        extract.flags = 0;
        extract.partialStartOffset = -1;
        extract.partialEndOffset = -1;

        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_GET_SELECTION, 0, 0));
        try {
            mQueryResult.take();
        } catch (InterruptedException e) {
            Log.e("GeckoAppJava", "IME: getExtractedText interrupted");
            return null;
        }
        extract.selectionStart = mSelectionStart;
        extract.selectionEnd = mSelectionStart + mSelectionLength;

        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_GET_TEXT, 0, Integer.MAX_VALUE));
        try {
            extract.startOffset = 0;
            extract.text = mQueryResult.take();

            if ((flags & GET_EXTRACTED_TEXT_MONITOR) != 0)
                mUpdateRequest = req;
            return extract;

        } catch (InterruptedException e) {
            Log.e("GeckoAppJava", "IME: getExtractedText interrupted");
            return null;
        }
    }

    @Override
    public CharSequence getTextAfterCursor(int length, int flags) {
        //Log.d("GeckoAppJava", "IME: getTextAfterCursor");

        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_GET_SELECTION, 0, 0));
        try {
            mQueryResult.take();
        } catch (InterruptedException e) {
            Log.e("GeckoAppJava", "IME: getTextBefore/AfterCursor interrupted");
            return null;
        }

        /* Compatible with both positive and negative length
            (no need for separate code for getTextBeforeCursor) */
        int textStart = mSelectionStart;
        int textLength = length;

        if (length < 0) {
          textStart += length;
          textLength = -length;
          if (textStart < 0) {
            textStart = 0;
            textLength = mSelectionStart;
          }
        }

        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_GET_TEXT, textStart, textLength));
        try {
            return mQueryResult.take();
        } catch (InterruptedException e) {
            Log.e("GeckoAppJava", "IME: getTextBefore/AfterCursor: Interrupted!");
            return null;
        }
    }

    @Override
    public CharSequence getTextBeforeCursor(int length, int flags) {
        //Log.d("GeckoAppJava", "IME: getTextBeforeCursor");

        return getTextAfterCursor(-length, flags);
    }

    @Override
    public boolean setComposingText(CharSequence text, int newCursorPosition) {
        //Log.d("GeckoAppJava", "IME: setComposingText");

        // Set new composing text
        mComposingText = text != null ? text.toString() : "";

        if (!mComposing) {
            // Get current selection
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_GET_SELECTION, 0, 0));
            try {
                mQueryResult.take();
            } catch (InterruptedException e) {
                Log.e("GeckoAppJava", "IME: setComposingText interrupted");
                return false;
            }
            // Make sure we are in a composition
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_COMPOSITION_BEGIN, 0, 0));
            mComposing = true;
            mCompositionStart = mSelectionLength >= 0 ?
                mSelectionStart : mSelectionStart + mSelectionLength;
        }

        // Set new selection
        // New selection should be within the composition
        mCompositionSelStart = newCursorPosition > 0 ? mComposingText.length() : 0;
        mCompositionSelLen = 0;

        // Handle composition text styles
        if (text != null && text instanceof Spanned) {
            Spanned span = (Spanned) text;
            int spanStart = 0, spanEnd = 0;
            boolean pastSelStart = false, pastSelEnd = false;

            do {
                int rangeType = GeckoEvent.IME_RANGE_CONVERTEDTEXT;
                int rangeStyles = 0, rangeForeColor = 0, rangeBackColor = 0;

                // Find next offset where there is a style transition
                spanEnd = span.nextSpanTransition(spanStart + 1, text.length(),
                    CharacterStyle.class);

                // We need to count the selection as a transition
                if (mCompositionSelLen >= 0) {
                    if (!pastSelStart && spanEnd >= mCompositionSelStart) {
                        spanEnd = mCompositionSelStart;
                        pastSelStart = true;
                    } else if (!pastSelEnd && spanEnd >=
                            mCompositionSelStart + mCompositionSelLen) {
                        spanEnd = mCompositionSelStart + mCompositionSelLen;
                        pastSelEnd = true;
                        rangeType = GeckoEvent.IME_RANGE_SELECTEDRAWTEXT;
                    }
                } else {
                    if (!pastSelEnd && spanEnd >=
                            mCompositionSelStart + mCompositionSelLen) {
                        spanEnd = mCompositionSelStart + mCompositionSelLen;
                        pastSelEnd = true;
                    } else if (!pastSelStart &&
                            spanEnd >= mCompositionSelStart) {
                        spanEnd = mCompositionSelStart;
                        pastSelStart = true;
                        rangeType = GeckoEvent.IME_RANGE_SELECTEDRAWTEXT;
                    }
                }
                // Empty range, continue
                if (spanEnd <= spanStart)
                    continue;

                // Get and iterate through list of span objects within range
                CharacterStyle styles[] = span.getSpans(
                    spanStart, spanEnd, CharacterStyle.class);

                for (CharacterStyle style : styles) {
                    if (style instanceof UnderlineSpan) {
                        // Text should be underlined
                        rangeStyles |= GeckoEvent.IME_RANGE_UNDERLINE;

                    } else if (style instanceof ForegroundColorSpan) {
                        // Text should be of a different foreground color
                        rangeStyles |= GeckoEvent.IME_RANGE_FORECOLOR;
                        rangeForeColor =
                            ((ForegroundColorSpan)style).getForegroundColor();

                    } else if (style instanceof BackgroundColorSpan) {
                        // Text should be of a different background color
                        rangeStyles |= GeckoEvent.IME_RANGE_BACKCOLOR;
                        rangeBackColor =
                            ((BackgroundColorSpan)style).getBackgroundColor();
                    }
                }

                // Add range to array, the actual styles are
                //  applied when IME_SET_TEXT is sent
                GeckoAppShell.sendEventToGecko(
                    new GeckoEvent(spanStart, spanEnd - spanStart,
                                   rangeType, rangeStyles,
                                   rangeForeColor, rangeBackColor));

                spanStart = spanEnd;
            } while (spanStart < text.length());
        } else {
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(0, text == null ? 0 : text.length(),
                               GeckoEvent.IME_RANGE_RAWINPUT,
                               GeckoEvent.IME_RANGE_UNDERLINE, 0, 0));
        }

        // Change composition (treating selection end as where the caret is)
        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(mCompositionSelStart + mCompositionSelLen, 0,
                           GeckoEvent.IME_RANGE_CARETPOSITION, 0, 0, 0,
                           mComposingText));
        return true;
    }

    @Override
    public boolean setSelection(int start, int end) {
        //Log.d("GeckoAppJava", "IME: setSelection");

        if (mComposing) {
            /* Translate to fake selection positions */
            start -= mCompositionStart;
            end -= mCompositionStart;

            if (start < 0)
                start = 0;
            else if (start > mComposingText.length())
                start = mComposingText.length();

            if (end < 0)
                end = 0;
            else if (end > mComposingText.length())
                end = mComposingText.length();

            mCompositionSelStart = start;
            mCompositionSelLen = end - start;
        } else {
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_SET_SELECTION,
                               start, end - start));
        }
        return true;
    }

    public boolean onKeyDel() {
        // Some IMEs don't update us on deletions
        // In that case we are not updated when a composition
        // is destroyed, and Bad Things happen

        if (!mComposing)
            return false;

        if (mComposingText.length() > 0) {
            mComposingText = mComposingText.substring(0,
                mComposingText.length() - 1);
            if (mComposingText.length() > 0)
                return false;
        }

        commitText(null, 1);
        return true;
    }

    public void notifyTextChange(InputMethodManager imm, String text,
                                 int start, int oldEnd, int newEnd) {
        //Log.d("GeckoAppJava", "IME: notifyTextChange");

        if (!text.contentEquals(GeckoApp.surfaceView.mEditable))
            GeckoApp.surfaceView.setupEditable(text);

        if (mUpdateRequest == null)
            return;

        mUpdateExtract.flags = 0;

        // We update from (0, oldEnd) to (0, newEnd) because some Android IMEs
        // assume that updates start at zero, according to jchen.
        mUpdateExtract.partialStartOffset = 0;
        mUpdateExtract.partialEndOffset = oldEnd;

        // Faster to not query for selection
        mUpdateExtract.selectionStart = newEnd;
        mUpdateExtract.selectionEnd = newEnd;

        mUpdateExtract.text = text.substring(0, newEnd);
        mUpdateExtract.startOffset = 0;

        imm.updateExtractedText(GeckoApp.surfaceView,
            mUpdateRequest.token, mUpdateExtract);
    }

    public void notifySelectionChange(InputMethodManager imm,
                                      int start, int end) {
        //Log.d("GeckoAppJava", "IME: notifySelectionChange");

        if (mComposing)
            imm.updateSelection(GeckoApp.surfaceView,
                mCompositionStart + mCompositionSelStart,
                mCompositionStart + mCompositionSelStart + mCompositionSelLen,
                mCompositionStart,
                mCompositionStart + mComposingText.length());
        else
            imm.updateSelection(GeckoApp.surfaceView, start, end, -1, -1);

        int maxLen = GeckoApp.surfaceView.mEditable.length();
        Selection.setSelection(GeckoApp.surfaceView.mEditable, 
                               Math.min(start, maxLen),
                               Math.min(end, maxLen));
    }

    public void reset() {
        mComposing = false;
        mComposingText = null;
        mUpdateRequest = null;
    }

    // TextWatcher
    public void onTextChanged(CharSequence s, int start, int before, int count)
    {
        GeckoAppShell.sendEventToGecko(
            new GeckoEvent(GeckoEvent.IME_SET_SELECTION, start, before));

        if (count == 0) {
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_DELETE_TEXT, 0, 0));
        } else {
            // Start and stop composition to force UI updates.
            finishComposingText();
            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_COMPOSITION_BEGIN, 0, 0));

            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(0, count,
                               GeckoEvent.IME_RANGE_RAWINPUT, 0, 0, 0,
                               s.subSequence(start, start + count).toString()));

            GeckoAppShell.sendEventToGecko(
                new GeckoEvent(GeckoEvent.IME_COMPOSITION_END, 0, 0));
        }
    }

    public void afterTextChanged(Editable s)
    {
    }

    public void beforeTextChanged(CharSequence s, int start, int count, int after)
    {
    }

    // Is a composition active?
    boolean mComposing;
    // Composition text when a composition is active
    String mComposingText;
    // Start index of the composition within the text body
    int mCompositionStart;
    /* During a composition, we should not alter the real selection,
        therefore we keep our own offsets to emulate selection */
    // Start of fake selection, relative to start of composition
    int mCompositionSelStart;
    // Length of fake selection
    int mCompositionSelLen;

    ExtractedTextRequest mUpdateRequest;
    final ExtractedText mUpdateExtract = new ExtractedText();

    int mSelectionStart, mSelectionLength;
    SynchronousQueue<String> mQueryResult;
}

