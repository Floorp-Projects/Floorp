/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "WebGLValidateStrings.h"

#include "nsString.h"
#include "WebGLContext.h"

namespace mozilla {
// The following code was taken from the WebKit WebGL implementation,
// which can be found here:
// http://trac.webkit.org/browser/trunk/Source/WebCore/html/canvas/WebGLRenderingContext.cpp?rev=93625#L121
// Note that some modifications were done to adapt it to Mozilla.
/****** BEGIN CODE TAKEN FROM WEBKIT ******/
bool IsValidGLSLCharacter(char16_t c)
{
    // Printing characters are valid except " $ ` @ \ ' DEL.
    if (c >= 32 && c <= 126 &&
        c != '"' && c != '$' && c != '`' && c != '@' && c != '\\' && c != '\'')
    {
         return true;
    }

    // Horizontal tab, line feed, vertical tab, form feed, carriage return
    // are also valid.
    if (c >= 9 && c <= 13) {
         return true;
    }

    return false;
}

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

bool
ValidateGLSLString(const nsAString& string, WebGLContext* webgl, const char* funcName)
{
    for (size_t i = 0; i < string.Length(); ++i) {
        if (!IsValidGLSLCharacter(string.CharAt(i))) {
           webgl->ErrorInvalidValue("%s: String contains the illegal character '%d'",
                                    funcName, string.CharAt(i));
           return false;
        }
    }

    return true;
}

bool
ValidateGLSLVariableName(const nsAString& name, WebGLContext* webgl, const char* funcName)
{
    if (name.IsEmpty())
        return false;

    const uint32_t maxSize = webgl->IsWebGL2() ? 1024 : 256;
    if (name.Length() > maxSize) {
        webgl->ErrorInvalidValue("%s: Identifier is %d characters long, exceeds the"
                                 " maximum allowed length of %d characters.",
                                 funcName, name.Length(), maxSize);
        return false;
    }

    if (!ValidateGLSLString(name, webgl, funcName))
        return false;

    nsString prefix1 = NS_LITERAL_STRING("webgl_");
    nsString prefix2 = NS_LITERAL_STRING("_webgl_");

    if (Substring(name, 0, prefix1.Length()).Equals(prefix1) ||
        Substring(name, 0, prefix2.Length()).Equals(prefix2))
    {
        webgl->ErrorInvalidOperation("%s: String contains a reserved GLSL prefix.",
                                     funcName);
        return false;
    }

    return true;
}

} // namespace mozilla
