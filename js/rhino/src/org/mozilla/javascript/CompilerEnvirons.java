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

import java.util.Hashtable;

public class CompilerEnvirons
{
    public CompilerEnvirons()
    {
        this.errorReporter = DefaultErrorReporter.instance;
        this.syntaxErrorCount = 0;
        this.fromEval = false;
        this.languageVersion = Context.VERSION_DEFAULT;
        this.generateDebugInfo = true;
        this.useDynamicScope = false;
        this.reservedKeywordAsIdentifier = false;
        this.allowMemberExprAsFunctionName = false;
        this.optimizationLevel = 0;
        this.generatingSource = true;
    }

    public void initFromContext(Context cx)
    {
        setErrorReporter(cx.getErrorReporter());
        this.languageVersion = cx.getLanguageVersion();
        useDynamicScope = cx.hasCompileFunctionsWithDynamicScope();
        generateDebugInfo = (!cx.isGeneratingDebugChanged()
                             || cx.isGeneratingDebug());
        this.reservedKeywordAsIdentifier
            = cx.hasFeature(Context.FEATURE_RESERVED_KEYWORD_AS_IDENTIFIER);
        this.allowMemberExprAsFunctionName
            = cx.hasFeature(Context.FEATURE_MEMBER_EXPR_AS_FUNCTION_NAME);

        this.optimizationLevel = cx.getOptimizationLevel();

        this.generatingSource = cx.isGeneratingSource();
        this.activationNames = cx.activationNames;
    }

    public final int getSyntaxErrorCount()
    {
        return syntaxErrorCount;
    }

    public final ErrorReporter getErrorReporter()
    {
        return errorReporter;
    }

    public void setErrorReporter(ErrorReporter errorReporter)
    {
        if (errorReporter == null) throw new IllegalArgumentException();
        this.errorReporter = errorReporter;
    }

    public final void reportSyntaxError(String message,
                                        String sourceName, int lineno,
                                        String lineText, int lineOffset)
    {
        ++syntaxErrorCount;
        if (fromEval) {
            // We're probably in an eval. Need to throw an exception.
            throw ScriptRuntime.constructError(
                "SyntaxError", message, sourceName,
                lineno, lineText, lineOffset);
        } else {
            getErrorReporter().error(message, sourceName, lineno,
                                     lineText, lineOffset);
        }
    }

    public final void reportSyntaxWarning(String message,
                                          String sourceName, int lineno,
                                          String lineText, int lineOffset)
    {
        getErrorReporter().warning(message, sourceName,
                                   lineno, lineText, lineOffset);
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

    public void setLanguageVersion(int languageVersion)
    {
        Context.checkLanguageVersion(languageVersion);
        this.languageVersion = languageVersion;
    }

    public final boolean isGenerateDebugInfo()
    {
        return generateDebugInfo;
    }

    public void setGenerateDebugInfo(boolean flag)
    {
        this.generateDebugInfo = flag;
    }

    public final boolean isUseDynamicScope()
    {
        return useDynamicScope;
    }

    public void setUseDynamicScope(boolean flag)
    {
        this.useDynamicScope = flag;
    }

    public final int getOptimizationLevel()
    {
        return optimizationLevel;
    }

    public void setOptimizationLevel(int level)
    {
        Context.checkOptimizationLevel(level);
        this.optimizationLevel = level;
    }

    public boolean isGeneratingSource()
    {
        return generatingSource;
    }

    /**
     * Specify whether or not source information should be generated.
     * <p>
     * Without source information, evaluating the "toString" method
     * on JavaScript functions produces only "[native code]" for
     * the body of the function.
     * Note that code generated without source is not fully ECMA
     * conformant.
     */
    public void setGeneratingSource(boolean generatingSource)
    {
        this.generatingSource = generatingSource;
    }

    final boolean isXmlAvailable()
    {
        return true;
    }

    private ErrorReporter errorReporter;
    private int syntaxErrorCount;

    private boolean fromEval;

    int languageVersion = Context.VERSION_DEFAULT;
    boolean generateDebugInfo = true;
    boolean useDynamicScope;
    boolean reservedKeywordAsIdentifier;
    boolean allowMemberExprAsFunctionName;
    private int optimizationLevel = 0;
    private boolean generatingSource = true;
    Hashtable activationNames;
}

