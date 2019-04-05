/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
(function(mod) {
  mod(require("../lib/codemirror"));
})(function(CodeMirror) {
  // CodeMirror uses an offscreen <textarea> to detect input.
  // Due to inconsistencies in the many browsers it supports, it simplifies things by
  // regularly checking if something is in the textarea, adding those characters to the
  // document, and then clearing the textarea.
  // This breaks assistive technology that wants to read from CodeMirror, because the
  // <textarea> that they interact with is constantly empty.
  // Because we target up-to-date Firefox, we can guarantee consistent input events.
  // This lets us leave the current line from the editor in our <textarea>.
  // CodeMirror still expects a mostly empty <textarea>, so we pass CodeMirror a fake
  // <textarea> that only contains the users input.
  CodeMirror.inputStyles.accessibleTextArea = class extends CodeMirror.inputStyles.textarea {
    /**
     * @override
     * @param {!Object} display
     */
    init(display) {
      super.init(display);
      this.textarea.addEventListener("compositionstart",
        this._onCompositionStart.bind(this));
    }

    _onCompositionStart() {
      if (this.textarea.selectionEnd === this.textarea.value.length) {
        return;
      }

      // CodeMirror always expects the caret to be at the end of the textarea
      // When in IME composition mode, clip the textarea to how CodeMirror expects it,
      // and then let CodeMirror do its thing.
      this.textarea.value = this.textarea.value.substring(0, this.textarea.selectionEnd);
      const length = this.textarea.value.length;
      this.textarea.setSelectionRange(length, length);
      this.prevInput = this.textarea.value;
    }

    /**
     * @override
     * @param {Boolean} typing
     */
    reset(typing) {
      if (
        typing ||
        this.contextMenuPending ||
        this.composing ||
        this.cm.somethingSelected()
      ) {
        super.reset(typing);
        return;
      }

      // When navigating around the document, keep the current visual line in the textarea.
      const cursor = this.cm.getCursor();
      let start, end;
      if (this.cm.options.lineWrapping) {
        // To get the visual line, compute the leftmost and rightmost character positions.
        const top = this.cm.charCoords(cursor, "page").top;
        start = this.cm.coordsChar({left: -Infinity, top});
        end = this.cm.coordsChar({left: Infinity, top});
      } else {
        // Limit the line to 1000 characters to prevent lag.
        const offset = Math.floor(cursor.ch / 1000) * 1000;
        start = {ch: offset, line: cursor.line};
        end = {ch: offset + 1000, line: cursor.line};
      }

      this.textarea.value = this.cm.getRange(start, end);
      const caretPosition = cursor.ch - start.ch;
      this.textarea.setSelectionRange(caretPosition, caretPosition);
      this.prevInput = this.textarea.value;
    }

    /**
     * @override
     * @return {boolean}
     */
    poll() {
      if (this.contextMenuPending || this.composing) {
        return super.poll();
      }

      const text = this.textarea.value;
      let start = 0;
      const length = Math.min(this.prevInput.length, text.length);

      while (start < length && this.prevInput[start] === text[start]) {
        ++start;
      }

      let end = 0;

      while (
        end < length - start &&
        this.prevInput[this.prevInput.length - end - 1] === text[text.length - end - 1]
      ) {
        ++end;
      }

      // CodeMirror expects the user to be typing into a blank <textarea>.
      // Pass a fake textarea into super.poll that only contains the users input.
      const placeholder = this.textarea;
      this.textarea = document.createElement("textarea");
      this.textarea.value = text.substring(start, text.length - end);
      this.textarea.setSelectionRange(
        placeholder.selectionStart - start,
        placeholder.selectionEnd - start
      );
      this.prevInput = "";
      const result = super.poll();
      this.prevInput = text;
      this.textarea = placeholder;
      return result;
    }
  };
});
