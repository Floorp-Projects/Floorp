/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

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
 * The Original Code is representation of compiler options for Rhino.
 *
 * The Initial Developer of the Original Code is
 * RUnit Software AS.
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): Igor Bukanov, igor@fastmail.fm
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

package org.mozilla.javascript;

public final class CompilerEnvirons
{
    public void initFromContext(Context cx, ErrorReporter syntaxErrorReporter)
    {
        setSyntaxErrorReporter(syntaxErrorReporter);
        this.languageVersion = cx.getLanguageVersion();
        useDynamicScope = cx.hasCompileFunctionsWithDynamicScope();
        generateDebugInfo = (!cx.isGeneratingDebugChanged()
                             || cx.isGeneratingDebug());
        this.reservedKeywordAsIdentifier
            = cx.hasFeature(Context.FEATURE_RESERVED_KEYWORD_AS_IDENTIFIER);
        this.allowMemberExprAsFunctionName
            = cx.hasFeature(Context.FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME);

        this.optimizationLevel = cx.getOptimizationLevel();
    }

    public final int getSyntaxErrorCount()
    {
        return syntaxErrorCount;
    }

    public void setSyntaxErrorReporter(ErrorReporter syntaxErrorReporter)
    {
        if (syntaxErrorReporter == null) Kit.argBug();
        this.syntaxErrorReporter = syntaxErrorReporter;
    }

    public final void reportSyntaxError(boolean isError,
                                        String messageProperty, Object[] args,
                                        String sourceName, int lineno,
                                        String line, int lineOffset)
    {
        String message = Context.getMessage(messageProperty, args);
        if (isError) {
            ++syntaxErrorCount;
            if (fromEval) {
                // We're probably in an eval. Need to throw an exception.
                throw ScriptRuntime.constructError(
                    "SyntaxError", message, sourceName,
                    lineno, line, lineOffset);
            } else {
                syntaxErrorReporter.error(message, sourceName,
                                          lineno, line, lineOffset);
            }
        } else {
            syntaxErrorReporter.warning(message, sourceName,
                                        lineno, line, lineOffset);
        }
    }

    public final boolean isFromEval()
    {
        return fromEval;
    }

    public void setFromEval(boolean fromEval)
    {
        this.fromEval = fromEval;
    }

    public final int getLanguageVersion()
    {
        return languageVersion;
    }

    public final boolean isGenerateDebugInfo()
    {
        return generateDebugInfo;
    }

    public final boolean isUseDynamicScope()
    {
        return useDynamicScope;
    }

    public final int getOptimizationLevel()
    {
        return optimizationLevel;
    }

    private ErrorReporter syntaxErrorReporter;
    private int syntaxErrorCount;

    private boolean fromEval;

    int languageVersion = Context.VERSION_DEFAULT;
    boolean generateDebugInfo;
    boolean useDynamicScope;
    boolean reservedKeywordAsIdentifier;
    boolean allowMemberExprAsFunctionName;
    private int optimizationLevel;
}

