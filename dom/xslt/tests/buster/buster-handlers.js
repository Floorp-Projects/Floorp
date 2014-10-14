/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var xalan_field;

function onLoad()
{
    view.tree = document.getElementById('out');
    view.boxObject = view.tree.boxObject;
    {  
        view.mIframe = document.getElementById('hiddenHtml');
        view.mIframe.webNavigation.allowPlugins = false;
        view.mIframe.webNavigation.allowJavascript = false;
        view.mIframe.webNavigation.allowMetaRedirects = false;
        view.mIframe.webNavigation.allowImages = false;
    }
    view.database = view.tree.database;
    view.builder = view.tree.builder.QueryInterface(nsIXULTemplateBuilder);
    view.builder.QueryInterface(nsIXULTreeBuilder);
    runItem.prototype.kDatabase = view.database;
    xalan_field = document.getElementById("xalan_rdf");
    var persistedUrl = xalan_field.getAttribute('url');
    if (persistedUrl) {
        view.xalan_url = persistedUrl;
        xalan_field.value = persistedUrl;
    }
    view.setDataSource();
    return true;
}

function onUnload()
{
    if (xalan_field)
        xalan_field.setAttribute('url', xalan_field.value);
}
