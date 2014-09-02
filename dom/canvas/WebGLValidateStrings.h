/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2011 Mozilla Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WEBGLVALIDATESTRINGS_H_
#define WEBGLVALIDATESTRINGS_H_

#include "WebGLContext.h"

namespace mozilla {

// The following code was taken from the WebKit WebGL implementation,
// which can be found here:
// http://trac.webkit.org/browser/trunk/Source/WebCore/html/canvas/WebGLRenderingContext.cpp?rev=93625#L121
// Note that some modifications were done to adapt it to Mozilla.
/****** BEGIN CODE TAKEN FROM WEBKIT ******/
    bool WebGLContext::ValidateGLSLCharacter(char16_t c)
    {
        // Printing characters are valid except " $ ` @ \ ' DEL.
        if (c >= 32 && c <= 126 &&
            c != '"' && c != '$' && c != '`' && c != '@' && c != '\\' && c != '\'')
        {
             return true;
        }

        // Horizontal tab, line feed, vertical tab, form feed, carriage return are also valid.
        if (c >= 9 && c <= 13) {
             return true;
        }

        return false;
    }

    // Strips comments from shader text. This allows non-ASCII characters
    // to be used in comments without potentially breaking OpenGL
    // implementations not expecting characters outside the GLSL ES set.
    class StripComments {
    public:
        explicit StripComments(const nsAString& aStr)
            : m_parseState(BeginningOfLine)
            , m_end(aStr.EndReading())
            , m_current(aStr.BeginReading())
            , m_position(0)
        {
            m_result.SetLength(aStr.Length());
            parse();
        }

        const nsTArray<char16_t>& result()
        {
            return m_result;
        }

        size_t length()
        {
            return m_position;
        }

    private:
        bool hasMoreCharacters()
        {
            return (m_current < m_end);
        }

        void parse()
        {
            while (hasMoreCharacters()) {
                process(current());
                // process() might advance the position.
                if (hasMoreCharacters())
                    advance();
            }
        }

        void process(char16_t);

        bool peek(char16_t& character)
        {
            if (m_current + 1 >= m_end)
                return false;
            character = *(m_current + 1);
            return true;
        }

        char16_t current()
        {
            //ASSERT(m_position < m_length);
            return *m_current;
        }

        void advance()
        {
            ++m_current;
        }

        bool isNewline(char16_t character)
        {
            // Don't attempt to canonicalize newline related characters.
            return (character == '\n' || character == '\r');
        }

        void emit(char16_t character)
        {
            m_result[m_position++] = character;
        }

        enum ParseState {
            // Have not seen an ASCII non-whitespace character yet on
            // this line. Possible that we might see a preprocessor
            // directive.
            BeginningOfLine,

            // Have seen at least one ASCII non-whitespace character
            // on this line.
            MiddleOfLine,

            // Handling a preprocessor directive. Passes through all
            // characters up to the end of the line. Disables comment
            // processing.
            InPreprocessorDirective,

            // Handling a single-line comment. The comment text is
            // replaced with a single space.
            InSingleLineComment,

            // Handling a multi-line comment. Newlines are passed
            // through to preserve line numbers.
            InMultiLineComment
        };

        ParseState m_parseState;
        const char16_t* m_end;
        const char16_t* m_current;
        size_t m_position;
        nsTArray<char16_t> m_result;
    };

    void StripComments::process(char16_t c)
    {
        if (isNewline(c)) {
            // No matter what state we are in, pass through newlines
            // so we preserve line numbers.
            emit(c);

            if (m_parseState != InMultiLineComment)
                m_parseState = BeginningOfLine;

            return;
        }

        char16_t temp = 0;
        switch (m_parseState) {
        case BeginningOfLine:
            // If it's an ASCII space.
            if (c <= ' ' && (c == ' ' || (c <= 0xD && c >= 0x9))) {
                emit(c);
                break;
            }

            if (c == '#') {
                m_parseState = InPreprocessorDirective;
                emit(c);
                break;
            }

            // Transition to normal state and re-handle character.
            m_parseState = MiddleOfLine;
            process(c);
            break;

        case MiddleOfLine:
            if (c == '/' && peek(temp)) {
                if (temp == '/') {
                    m_parseState = InSingleLineComment;
                    emit(' ');
                    advance();
                    break;
                }

                if (temp == '*') {
                    m_parseState = InMultiLineComment;
                    // Emit the comment start in case the user has
                    // an unclosed comment and we want to later
                    // signal an error.
                    emit('/');
                    emit('*');
                    advance();
                    break;
                }
            }

            emit(c);
            break;

        case InPreprocessorDirective:
            // No matter what the character is, just pass it
            // through. Do not parse comments in this state. This
            // might not be the right thing to do long term, but it
            // should handle the #error preprocessor directive.
            emit(c);
            break;

        case InSingleLineComment:
            // The newline code at the top of this function takes care
            // of resetting our state when we get out of the
            // single-line comment. Swallow all other characters.
            break;

        case InMultiLineComment:
            if (c == '*' && peek(temp) && temp == '/') {
                emit('*');
                emit('/');
                m_parseState = MiddleOfLine;
                advance();
                break;
            }

            // Swallow all other characters. Unclear whether we may
            // want or need to just emit a space per character to try
            // to preserve column numbers for debugging purposes.
            break;
        }
    }

/****** END CODE TAKEN FROM WEBKIT ******/

} // end namespace mozilla

#endif // WEBGLVALIDATESTRINGS_H_
