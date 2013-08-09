<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Guides #

This page lists more theoretical in-depth articles about the SDK.

<hr>

<h2><a name="contributors-guide">Contributor's Guide</a></h2>

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="dev-guide/guides/contributors-guide/getting-started.html">Getting Started</a></h4>
      Learn how to contribute to the SDK: getting the code, opening/taking a
      bug, filing a patch, getting reviews, and getting help.
    </td>

    <td>
      <h4><a href="dev-guide/guides/contributors-guide/private-properties.html">Private Properties</a></h4>
      Learn how private properties can be implemented in JavaScript using
      prefixes, closures, and WeakMaps, and how the SDK supports private
      properties by using namespaces (which are a generalization of WeakMaps).
    </td>

  </tr>
  <tr>
    <td>
      <h4><a href="dev-guide/guides/contributors-guide/modules.html">Modules</a></h4>
      Learn about the module system used by the SDK (which is based on the
      CommonJS specification), how sandboxes and compartments can be used to
      improve security, and about the built-in SDK module loader, known as
      Cuddlefish.
    </td>

    <td>
      <h4><a href="dev-guide/guides/contributors-guide/content-processes.html">Content Processes</a></h4>
      The SDK was designed to work in an environment where the code to
      manipulate web content runs in a different process from the main add-on
      code. This article highlights the main features of that design.
    </td>

  </tr>
  <tr>
    <td>
      <h4><a href="dev-guide/guides/contributors-guide/classes-and-inheritance.html">Classes and Inheritance</a></h4>
      Learn how classes and inheritance can be implemented in JavaScript, using
      constructors and prototypes, and about the helper functions provided by
      the SDK to simplify this.
    </td>

    <td>
    </td>

  </tr>
</table>

<h2><a name="sdk-infrastructure">SDK Infrastructure</a></h2>

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="dev-guide/guides/modules.html">Module structure of the SDK</a></h4>
      The SDK, and add-ons built using it, are of composed from reusable JavaScript modules. This
      explains what these modules are, how to load modules, and how the SDK's module
      tree is structured.
    </td>

    <td>
      <h4><a href="dev-guide/guides/program-id.html">Program ID</a></h4>
      The Program ID is a unique identifier for your add-on. This guide
      explains how it's created, what it's used for and how to define your
      own.
    </td>

  </tr>
  <tr>

    <td>
    </td>

    <td>
      <h4><a href="dev-guide/guides/firefox-compatibility.html">Firefox compatibility</a></h4>
      Working out which Firefox releases a given SDK release is
      compatible with, and dealing with compatibility problems.
    </td>

  </tr>

  <tr>

    <td>
      <h4><a href="dev-guide/guides/stability.html">SDK API lifecycle</a></h4>
      Definition of the lifecycle for the SDK's APIs, including the stability
      ratings for APIs.
    </td>

    <td>
    </td>

  </tr>

</table>

<hr>

<h2><a name="sdk-idioms">SDK Idioms</a></h2>

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="dev-guide/guides/events.html">Working With Events</a></h4>
      Write event-driven code using the the SDK's event emitting framework.
    </td>

    <td>
      <h4><a href="dev-guide/guides/two-types-of-scripts.html">Two Types of Scripts</a></h4>
      This article explains the differences between the APIs
      available to your main add-on code and those available
      to content scripts.
    </td>

  </tr>

</table>

<hr>

<h2><a name="content-scripts">Content Scripts</a></h2>

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="dev-guide/guides/content-scripts/index.html">Introducing content scripts</a></h4>
      An overview of content scripts.
    </td>

    <td>
      <h4><a href="dev-guide/guides/content-scripts/loading.html">Loading content scripts</a></h4>
      Load content scripts into web pages, specified either as strings
      or in separate files, and how to control the point at which they are
      executed.
    </td>

  </tr>

  <tr>
    <td>
      <h4><a href="dev-guide/guides/content-scripts/accessing-the-dom.html">Accessing the DOM</a></h4>
	  Detail about the access content scripts get to the DOM.
    </td>

    <td>
      <h4><a href="dev-guide/guides/content-scripts/communicating-with-other-scripts.html">Communicating with other scripts</a></h4>
	  Detail about how content scripts can communicate with "main.js", with other
	  content scripts, and with scripts loaded by the web page itself.
    </td>

  </tr>

  <tr>

    <td>
      <h4><a href="dev-guide/guides/content-scripts/using-port.html">Using "port"</a></h4>
      Communicating between a content script and the rest of your add-on
      using the <code>port</code> object.
    </td>


    <td>
      <h4><a href="dev-guide/guides/content-scripts/using-postmessage.html">Using "postMessage()"</a></h4>
      Communicating between a content script and the rest of your add-on
      using the <code>postMessage()</code> API, and a comparison between
      this technique and the <code>port</code> object.
    </td>

  </tr>

  <tr>

    <td>
      <h4><a href="dev-guide/guides/content-scripts/cross-domain.html">Cross-domain content scripts</a></h4>
      How to enable content scripts to interact with content served from different domains.
    </td>


    <td>
      <h4><a href="dev-guide/guides/content-scripts/reddit-example.html">Reddit example</a></h4>
      A simple add-on which uses content scripts.
    </td>

  </tr>

</table>

<hr>

<h2><a name="xul-migration">XUL Migration</a></h2>

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="dev-guide/guides/xul-migration.html">XUL Migration Guide</a></h4>
      Techniques to help port a XUL add-on to the SDK.
    </td>

    <td>
      <h4><a href="dev-guide/guides/sdk-vs-xul.html">XUL versus the SDK</a></h4>
      A comparison of the strengths and weaknesses of the SDK,
      compared to traditional XUL-based add-ons.
    </td>

  </tr>
  <tr>

    <td>
      <h4><a href="dev-guide/guides/library-detector.html">Porting Example</a></h4>
      A walkthrough of porting a relatively simple XUL-based
      add-on to the SDK.
    </td>

    <td>
    </td>

  </tr>

</table>
