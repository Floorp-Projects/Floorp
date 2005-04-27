/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Original Code is The JavaScript Debugger.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, <rginda@netscape.com>, original author
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

function MessageManager(entities)
{
    const UC_CTRID = "@mozilla.org/intl/scriptableunicodeconverter";
    const nsIUnicodeConverter = 
        Components.interfaces.nsIScriptableUnicodeConverter;

    this.ucConverter =
        Components.classes[UC_CTRID].getService(nsIUnicodeConverter);
    this.defaultBundle = null;
    this.bundleList = new Array();
    // Provide a fallback so we don't break getMsg and related constants later.
    this.entities = entities || {};
}

MessageManager.prototype.loadBrands =
function mm_loadbrands()
{
    var entities = this.entities;
    var app = getService("@mozilla.org/xre/app-info;1", "nsIXULAppInfo");
    if (app)
    {
        // Use App info if possible
        entities.brandShortName = app.name;
        entities.brandFullName = app.name + " " + app.version;
        entities.brandVendorName = app.vendor;
        return;
    }

    var brandBundle;
    var path = "chrome://branding/locale/brand.properties";
    try
    {
        brandBundle = this.addBundle(path);
    }
    catch (exception)
    {
        // May be an older mozilla version, try another location.
        path = "chrome://global/locale/brand.properties";
        brandBundle = this.addBundle(path);
    }

    entities.brandShortName = brandBundle.GetStringFromName("brandShortName");
    entities.brandVendorName = brandBundle.GetStringFromName("vendorShortName");
    // Not all versions of Suite / Fx have this defined; Cope:
    try
    {
        entities.brandFullName = brandBundle.GetStringFromName("brandFullName");
    }
    catch(exception)
    {
        entities.brandFullName = entities.brandShortName;
    }

    // Remove all of this junk, or it will be the default bundle for getMsg...
    this.bundleList.pop();
}

MessageManager.prototype.addBundle = 
function mm_addbundle(bundlePath, targetWindow)
{
    var bundle = srGetStrBundle(bundlePath);
    this.bundleList.push(bundle);

    // The bundle will load if the file doesn't exist. This will fail though.
    // We want to be clean and remove the bundle again.
    try
    {
        this.importBundle(bundle, targetWindow, this.bundleList.length - 1);
    }
    catch (exception)
    {
        // Clean up and return the exception.
        this.bundleList.pop();
        throw exception;
    }
    return bundle;
}

MessageManager.prototype.importBundle =
function mm_importbundle(bundle, targetWindow, index)
{
    var me = this;
    function replaceEntities(matched, entity)
    {
        if (entity in me.entities)
            return me.entities[entity];

        return matched;
    };
    const nsIPropertyElement = Components.interfaces.nsIPropertyElement;

    if (!targetWindow)
        targetWindow = window;

    if (typeof index == "undefined")
        index = arrayIndexOf(this.bundleList, bundle);
    
    var pfx;
    if (index == 0)
        pfx = "";
    else
        pfx = index + ":";

    var enumer = bundle.getSimpleEnumeration();

    while (enumer.hasMoreElements())
    {
        var prop = enumer.getNext().QueryInterface(nsIPropertyElement);
        var ary = prop.key.match (/^(msg|msn)/);
        if (ary)
        {
            var constValue;
            var constName = prop.key.toUpperCase().replace (/\./g, "_");
            if (ary[1] == "msn" || prop.value.search(/%(\d+\$)?s/i) != -1)
                constValue = pfx + prop.key;
            else
                constValue = prop.value.replace (/^\"/, "").replace (/\"$/, "");

            constValue = constValue.replace(/\&(\w+)\;/g, replaceEntities);
            targetWindow[constName] = constValue;
        }
    }

    if (this.bundleList.length == 1)
        this.defaultBundle = bundle;
}

MessageManager.prototype.checkCharset =
function mm_checkset(charset)
{
    try
    {
        this.ucConverter.charset = charset;
    }
    catch (ex)
    {
        return false;
    }
    
    return true;
}

MessageManager.prototype.toUnicode =
function mm_tounicode(msg, charset)
{
    if (!charset)
        return msg;
    
    try
    {
        this.ucConverter.charset = charset;
        msg = this.ucConverter.ConvertToUnicode(msg);
    }
    catch (ex)
    {
        //dd ("caught exception " + ex + " converting " + msg + " to charset " +
        //    charset);
    }

    return msg;
}

MessageManager.prototype.fromUnicode =
function mm_fromunicode(msg, charset)
{
    if (!charset)
        return msg;

    try
    {
        // This can actually fail in bizare cases. Cope.
        if (charset != this.ucConverter.charset)
            this.ucConverter.charset = charset;

        if ("Finish" in this.ucConverter)
        {
            msg = this.ucConverter.ConvertFromUnicode(msg) +
                this.ucConverter.Finish();
        }
        else
        {
            msg = this.ucConverter.ConvertFromUnicode(msg + " ");
            msg = msg.substr(0, msg.length - 1);
        }
    }
    catch (ex)
    {
        //dd ("caught exception " + ex + " converting " + msg + " to charset " +
        //    charset);
    }
    
    return msg;
}

MessageManager.prototype.getMsg = 
function mm_getmsg (msgName, params, deflt)
{
    try
    {    
        var bundle;
        var ary = msgName.match (/(\d+):(.+)/);
        if (ary)
        {
            return (this.getMsgFrom(this.bundleList[ary[1]], ary[2], params,
                                    deflt));
        }
        
        return this.getMsgFrom(this.bundleList[0], msgName, params, deflt);
    }
    catch (ex)
    {
        ASSERT (0, "Caught exception getting message: " + msgName + "/" +
                params);
        return deflt ? deflt : msgName;
    }
}

MessageManager.prototype.getMsgFrom =
function mm_getfrom (bundle, msgName, params, deflt)
{
    var me = this;
    function replaceEntities(matched, entity)
    {
        if (entity in me.entities)
            return me.entities[entity];

        return matched;
    };

    try 
    {
        var rv;
        
        if (params && isinstance(params, Array))
            rv = bundle.formatStringFromName (msgName, params, params.length);
        else if (params || params == 0)
            rv = bundle.formatStringFromName (msgName, [params], 1);
        else
            rv = bundle.GetStringFromName (msgName);
        
        /* strip leading and trailing quote characters, see comment at the
         * top of venkman.properties.
         */
        rv = rv.replace(/^\"/, "");
        rv = rv.replace(/\"$/, "");
        rv = rv.replace(/\&(\w+)\;/g, replaceEntities);

        return rv;
    }
    catch (ex)
    {
        if (typeof deflt == "undefined")
        {
            ASSERT (0, "caught exception getting value for ``" + msgName +
                    "''\n" + ex + "\n");
            return msgName;
        }
        return deflt;
    }

    return null;
}
