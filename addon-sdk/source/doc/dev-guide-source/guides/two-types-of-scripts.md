<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Two Types of Scripts #

On the web, JavaScript executes in the context of a web page, and has access to
that page's DOM content. This enables you to call functions like:

    window.alert("Hello there");

In an add-on's main scripts you can't do that, because the add-on code does
not execute in the context of a page, and the DOM is therefore not available.
If you need to access the DOM of a particular page, you need to use a
content script.

So there are two distinct sorts of JavaScript scripts you might include
in your add-on and they have access to different sets of APIs. In the SDK
documentation we call one sort "add-on code" and the other sort "content
scripts".

## Add-on Code ##

This is the place where the main logic of your add-on is implemented.

Your add-on is implemented as a collection of one or more
[CommonJS modules](dev-guide/guides/modules.html). Each module
is supplied as a script stored under the `lib` directory under your add-on's
root directory.

Minimally you'll have a single module implemented by a script called
"main.js", but you can include additional modules in `lib`, and import them
using the `require()` function. To learn how to implement and import your own
modules, see the tutorial on
[Implementing Reusable Modules](dev-guide/tutorials/reusable-modules.html).

## Content Scripts ##

While your add-on will always have a "main.js" module, you will only need
to write content scripts if your add-on needs to manipulate web content.
Content scripts are injected into web pages using APIs defined by some of the
SDK's modules such as `page-mod`, `panel` and `widget`.

Content scripts may be supplied as literal strings or maintained in separate
files and referenced by filename. If they are stored in separate files you
should store them under the `data` directory under your add-on's root.

To learn all about content scripts read the
[Working with Content Scripts](dev-guide/guides/content-scripts/index.html)
guide.

## API Access for Add-on Code and Content Scripts ##

The table below summarizes the APIs that are available to each type of
script.

<table>
  <colgroup>
    <col width="70%">
    <col width="15%">
    <col width="15%">
  </colgroup>
  <tr>
    <th>API</th>
    <th>Add-on code</th>
    <th>Content script</th>
  </tr>

  <tr>
    <td>The global objects defined in the core JavaScript language, such as
<code>Math</code>, <code>Array</code>, and <code>JSON</code>. See the
<a href= "https://developer.mozilla.org/en/JavaScript/Reference/Global_Objects">reference at MDN</a>.
    </td>
    <td class="check">✔</td>
    <td class="check">✔</td>
  </tr>

  <tr>
    <td><p>The <code>require()</code> and <code>exports</code> globals defined
by version 1.0 of the
<a href="http://wiki.commonjs.org/wiki/Modules/1.0">CommonJS Module Specification</a>.
You use <code>require()</code> to import functionality from another module,
and <code>exports</code> to export functionality from your module.</p>
If <code>require()</code> is available, then so are the modules supplied in the
SDK.
    </td>
    <td class="check">✔</td>
    <td class="cross">✘</td>
  </tr>

  <tr>
    <td>The <a href="dev-guide/console.html">console</a>
global supplied by the SDK.
    </td>
    <td class="check">✔</td>
    <td class="check">✔</td>
  </tr>

  <tr>
    <td>Globals defined by the
<a href="http://dev.w3.org/html5/spec/Overview.html">HTML5</a> specification,
such as <code>window</code>, <code>document</code>, and
<code>localStorage</code>.
    </td>
    <td class="cross">✘</td>
    <td class="check">✔</td>
  </tr>

  <tr>
    <td>The <code>self</code> global, used for communicating between content
scripts and add-on code. See the guide to
<a href="dev-guide/guides/content-scripts/using-port.html">communicating with content scripts</a>
for more details.
    </td>
    <td class="cross">✘</td>
    <td class="check">✔</td>
  </tr>

</table>
