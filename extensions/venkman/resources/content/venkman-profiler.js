/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is The JavaScript Debugger
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 * Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *  Robert Ginda, <rginda@netscape.com>, original author
 *
 */

function initProfiler ()
{
    var prefs =
        [
         ["profile.template.xml", "chrome://venkman/locale/profile.xml.tpl"],
         ["profile.template.html", "chrome://venkman/locale/profile.html.tpl"],
         ["profile.template.csv", "chrome://venkman/locale/profile.csv.tpl"],
         ["profile.template.txt", "chrome://venkman/locale/profile.txt.tpl"],
         ["profile.ranges.default", "1000000, 5000, 2500, 1000, 750, 500, 250," +
          "100, 75, 50, 25, 10, 7.5, 5, 2.5, 1, 0.75, 0.5, 0.25"]
        ];

    console.prefManager.addPrefs(prefs);
}

function ProfileReport (reportTemplate, file, rangeList, scriptInstanceList)
{
    this.reportTemplate = reportTemplate;
    this.file = file;
    this.rangeList = rangeList;
    this.scriptInstanceList = scriptInstanceList;
    this.key = "total";
}

console.profiler = new Object();

console.profiler.__defineSetter__ ("enabled", pro_setenable);
function pro_setenable(state)
{
    if (state)
        console.jsds.flags |= COLLECT_PROFILE_DATA;
    else
        console.jsds.flags &= ~COLLECT_PROFILE_DATA;
}

console.profiler.__defineGetter__ ("enabled", pro_getenable);
function pro_getenable(state)
{
    return Boolean(console.jsds.flags & COLLECT_PROFILE_DATA);
}

console.profiler.summarizeScriptWrapper =
function pro_sumscript (scriptWrapper, key)
{
    var ex;

    try
    {
        var jsdScript = scriptWrapper.jsdScript;
        if (!jsdScript.isValid)
            return null;
        
        var ccount = jsdScript.callCount;
        if (!ccount)
            return null;
        
        var tot_ms = roundTo(jsdScript.totalExecutionTime, 2);
        var min_ms = roundTo(jsdScript.minExecutionTime, 2);
        var max_ms = roundTo(jsdScript.maxExecutionTime, 2);
        var avg_ms = roundTo(jsdScript.totalExecutionTime / ccount, 2);
        var recurse = jsdScript.maxRecurseDepth;

        var summary = new Object();
        summary.total = tot_ms;
        summary.ccount = ccount;
        summary.avg = avg_ms;
        summary.min = min_ms;
        summary.max = max_ms;
        summary.recurse = recurse;
        summary.url = jsdScript.fileName;
        summary.file = getFileFromPath(summary.url);
        summary.base = jsdScript.baseLineNumber;
        summary.end = summary.base + jsdScript.lineExtent;
        summary.fun = scriptWrapper.functionName;
        summary.str = getMsg(MSN_FMT_PROFILE_STR,
                             [summary.fun, summary.base, summary.end, ccount,
                              (summary.recurse ?
                               getMsg(MSN_FMT_PROFILE_RECURSE, recurse) : ""),
                              tot_ms, min_ms, max_ms, avg_ms]);
        summary.key = summary[key];
        return summary;
    }
    catch (ex)
    {
        /* This function is called under duress, and the script representd
         * by |s| may get collected at any point.  When that happens,
         * attempting to access to the profile data will throw this
         * exception.
         */
        if ("result" in ex &&
            ex.result == Components.results.NS_ERROR_NOT_AVAILABLE)
        {
            display (getMsg(MSN_PROFILE_LOST, formatScript(jsdScript)), MT_WARN);
        }
        else
        {
            dd ("rethrow");
            dd (dumpObjectTree(ex));
            throw ex;
        }    
    }

    return null;
}

console.profiler.summarizeScriptInstance =
function pro_suminst (scriptInstance, key)
{
    var summaryList = new Array();
    var summary;
    
    if (scriptInstance.topLevel)
        summary = this.summarizeScriptWrapper (scriptInstance.topLevel, key);
    if (summary)
        summaryList.push (summary);

    for (var f in scriptInstance.functions)
    {
        summary = this.summarizeScriptWrapper(scriptInstance.functions[f], key);
        if (summary)
            summaryList.push(summary);
    }

    return summaryList;
}

console.profiler.generateReportSection =
function pro_rptinst (profileReport, scriptInstance, sectionData)
{
    function keyCompare (a, b)
    {
        if (a.key < b.key)
            return 1;
        if (a.key > b.key)
            return -1;
        return 0;
    };

    function scale(K, x) { return roundTo(K * x, 2); };

    const MAX_WIDTH = 90;

    var summaryList = this.summarizeScriptInstance (scriptInstance,
                                                    profileReport.key);
    if (!summaryList.length)
        return false;

    summaryList = summaryList.sort(keyCompare);

    var file = profileReport.file;
    var reportTemplate = profileReport.reportTemplate;
    var rangeList = profileReport.rangeList ? profileReport.rangeList : [1, 0];
    var finalRangeIndex = rangeList.length - 1;
    var previousRangeIndex;
    var rangeIter = 0;
    var rangeIndex = 0;
    var K = 1;
    var i;
    
    if (typeof summaryList[0].key == "number")
    {
        for (i = 0; i < rangeList.length; ++i)
            rangeList[i] = Number(rangeList[i]);
    }

    if ("sectionHeader" in reportTemplate)
    {
        file.write(replaceStrings(reportTemplate.sectionHeader,
                                  sectionData));
    }

    for (i = 0; i < summaryList.length; ++i)
    {
        var summary = summaryList[i];
        var rangeData;
        
        while (summary.key &&
               rangeIndex < finalRangeIndex &&
               summary.key < rangeList[rangeIndex])
        {
            ++rangeIndex;
        }

        if (previousRangeIndex != rangeIndex)
        {
            if (rangeIter > 0 && ("rangeFooter" in reportTemplate))
            {
                file.write(replaceStrings(reportTemplate.rangeFooter,
                                          rangeData));
            }

            var maxRange = (rangeIndex > 0 ?
                            rangeList[rangeIndex - 1] : summary.key);
            var minRange = (rangeIndex < finalRangeIndex ? 
                            rangeList[rangeIndex + 1] : summary.key);
            K = MAX_WIDTH / maxRange;

            rangeData = {
                "\\$range-min"        : minRange,
                "\\$range-max"        : maxRange,
                "\\$range-number-prev": rangeIter > 0 ? rangeIter - 1 : 0,
                "\\$range-number-next": rangeIter + 1,
                "\\$range-number"     : rangeIter,
                "__proto__"           : sectionData
            };

            if ("rangeHeader" in reportTemplate)
            {
                file.write(replaceStrings(reportTemplate.rangeHeader,
                                          rangeData));
            }

            previousRangeIndex = rangeIndex;
            ++rangeIter;
        }

        var summaryData = {
            "\\$item-number-next": i + 1,
            "\\$item-number-prev": i - 1,
            "\\$item-number"     : i,
            "\\$item-name"       : summary.url,
            "\\$item-summary"    : fromUnicode(summary.str, MSG_REPORT_CHARSET),
            "\\$item-min-pct"    : scale(K, summary.min),
            "\\$item-below-pct"  : scale(K, summary.avg - summary.min),
            "\\$item-above-pct"  : scale(K, summary.max - summary.avg),
            "\\$max-time"        : summary.max,
            "\\$min-time"        : summary.min,
            "\\$avg-time"        : summary.avg,
            "\\$total-time"      : summary.total,
            "\\$call-count"      : summary.ccount,
            "\\$recurse-depth"   : summary.recurse,
            "\\$function-name"   : fromUnicode(summary.fun, MSG_REPORT_CHARSET),
            "\\$start-line"      : summary.base,
            "\\$end-line"        : summary.end,
            "__proto__"          : rangeData
        };

        if ("itemBody" in reportTemplate)
            file.write(replaceStrings(reportTemplate.itemBody, summaryData));
    }

    if ("rangeFooter" in reportTemplate)
    {
        /* close the final range */
        file.write(replaceStrings(reportTemplate.rangeFooter,
                                  rangeData));
    }

    if ("sectionFooter" in reportTemplate)
    {
        file.write(replaceStrings(reportTemplate.sectionFooter,
                                  sectionData));
    }

    return true;
}

console.profiler.generateReport =
function pro_rptall (profileReport)
{
    var profiler = this;
    var sectionCount = 0;
    
    function generateReportChunk (i)
    {
        /* we build the report in chunks, with setTimeouts in between,
         * so the UI can come up for air. */
        var scriptInstance = profileReport.scriptInstanceList[i];
        var url = scriptInstance.url;
        
        var sectionData = {
            "\\$section-number-prev": (sectionCount > 0) ? sectionCount - 1 : 0,
            "\\$section-number-next": sectionCount + 1,
            "\\$section-number"     : sectionCount,
            "\\$section-link"       : (url ? "<a class='section-link' href='" +
                                       url + "'>" + url + "</a>" : MSG_VAL_NA),
            "\\$full-url"           : url,
            "\\$file-name"          : getFileFromPath(url),
            "__proto__"             : reportData
        };
        
        var hadData = profiler.generateReportSection (profileReport,
                                                      scriptInstance,
                                                      sectionData);
        if (++i == profileReport.scriptInstanceList.length)
        {
            if ("reportFooter" in reportTemplate)
                file.write(replaceStrings(reportTemplate.reportFooter,
                                          reportData));
            if ("onComplete" in profileReport)
                profileReport.onComplete(i);
        }
        else
        {
            if (hadData)
            {
                ++sectionCount;
                console.status = getMsg(MSN_PROFILE_SAVING, [i, last]);
                setTimeout (generateReportChunk, 10, i);                
            }
            else
            {
                generateReportChunk (i);
            }
        }   
    };
    
    var reportData = {
        "\\$report-charset": MSG_REPORT_CHARSET,
        "\\$full-date"     : String(Date()),
        "\\$user-agent"    : navigator.userAgent,
        "\\$venkman-agent" : console.userAgent,
        "\\$sort-key"      : profileReport.key
    };

    var reportTemplate = profileReport.reportTemplate;
    var file = profileReport.file;
    
    if ("reportHeader" in reportTemplate)
        file.write(replaceStrings(reportTemplate.reportHeader, reportData));

    var length = profileReport.scriptInstanceList.length;
    var last = length - 1;

    generateReportChunk (0);
}

console.profiler.loadTemplate =
function pro_load (url)
{
    var lines = loadURLNow(url);
    if (!lines)
        return null;

    var sections = {
         "reportHeader"  : /@-section-start/mi,
         "sectionHeader" : /@-range-start/mi,
         "rangeHeader"   : /@-item-start/mi,
         "itemBody"      : /@-item-end/mi,
         "rangeFooter"   : /@-range-end/mi,
         "sectionFooter" : /@-section-end/mi,
         "reportFooter"  : 0
    };

    var reportTemplate = parseSections (lines, sections);
    
    //dd(dumpObjectTree (reportTemplate));
    return reportTemplate;
}

