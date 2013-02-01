<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

# Tutorials #

This page lists practical explanations of how to develop add-ons with
the SDK. The tutorials don't yet cover all the high-level APIs: see the sidebar
on the left for the full list of APIs.

<hr>

<h2><a name="getting-started">Getting Started</a></h2>

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/installation.html">Installation</a></h4>
      Download, install, and initialize the SDK on Windows, OS X and Linux.
    </td>

    <td>
      <h4><a href="dev-guide/tutorials/getting-started-with-cfx.html">Getting started with cfx</a></h4>
      The basic <code>cfx</code> commands you need to start creating add-ons.
    </td>

  </tr>
  <tr>

    <td>
      <h4><a href="dev-guide/tutorials/troubleshooting.html">Troubleshooting</a></h4>
      Some pointers for fixing common problems and getting more help.
    </td>

    <td>
    </td>

  </tr>

</table>

<hr>

<h2><a name="create-user-interfaces">Create User Interfaces</a></h2>

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/adding-toolbar-button.html">Add a toolbar button</a></h4>
      Attach a button to the Firefox Add-on toolbar.
    </td>

    <td>
      <h4><a href="dev-guide/tutorials/display-a-popup.html">Display a popup</a></h4>
      Display a popup dialog implemented with HTML and JavaScript.
    </td>

  </tr>

  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/adding-menus.html">Add a menu item to Firefox</a></h4>
      Add items to Firefox's main menus.
    </td>

    <td>
      <h4><a href="dev-guide/tutorials/add-a-context-menu-item.html">Add a context menu item</a></h4>
      Add items to Firefox's context menu.
    </td>

  </tr>

</table>

<hr>

<h2><a name="interact-with-the-browser">Interact with the Browser</a></h2>

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/open-a-web-page.html">Open a web page</a></h4>
      Open a web page in a new browser tab or window using the
      <code><a href="modules/sdk/tabs.html">tabs</a></code> module, and access its content.
    </td>

    <td>
      <h4><a href="dev-guide/tutorials/list-open-tabs.html">Get the list of open tabs</a></h4>
      Use the <code><a href="modules/sdk/tabs.html">tabs</a></code>
      module to iterate through the currently open tabs, and access their content.
    </td>

  </tr>

  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/listen-for-page-load.html">Listen for page load</a></h4>
      Use the <code><a href="modules/sdk/tabs.html">tabs</a></code>
      module to get notified when new web pages are loaded, and access their content.
    </td>

    <td>
    </td>

  </tr>

</table>

<hr>

<h2><a name="modify-web-pages">Modify Web Pages</a></h2>

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/modifying-web-pages-url.html">Modify web pages based on URL</a></h4>
      Create filters for web pages based on their URL: whenever a web page
      whose URL matches the filter is loaded, execute a specified script in it.
    </td>

    <td>
      <h4><a href="dev-guide/tutorials/modifying-web-pages-tab.html">Modify the active web page</a></h4>
      Dynamically load a script into the currently active web page.
    </td>

  </tr>

</table>

<hr>

<h2><a name="development-techniques">Development Techniques</a></h2>

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/logging.html">Logging</a></h4>
      Log messages to the console for diagnostic purposes.
    </td>

    <td>
      <h4><a href="dev-guide/tutorials/load-and-unload.html">Listen for load and unload</a></h4>
      Get notifications when your add-on is loaded or unloaded by Firefox,
      and pass arguments into your add-on from the command line.
    </td>

  </tr>

  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/reusable-modules.html">Creating third-party modules</a></h4>
      Structure your add-on in separate modules to make it easier to develop, debug, and maintain.
      Create reusable packages containing your modules, so other add-on developers can use them too.
    </td>

    <td>
      <h4><a href="dev-guide/tutorials/adding-menus.html">Using third-party modules</a></h4>
      Install and use additional modules which don't ship with the SDK itself.
    </td>

  </tr>

  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/unit-testing.html">Unit testing</a></h4>
      Writing and running unit tests using the SDK's test framework.
    </td>

    <td>
      <h4><a href="dev-guide/tutorials/l10n.html">Localization</a></h4>
      Writing localizable code.
    </td>

  </tr>

  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/chrome.html">Chrome authority</a></h4>
      Get access to the <a href="https://developer.mozilla.org/en/Components_object">Components</a>
      object, enabling your add-on to load and use any XPCOM object.
    </td>

    <td>
      <h4><a href="dev-guide/tutorials/mobile.html">Mobile development</a></h4>
      Get set up to develop add-ons for Firefox Mobile on Android.
    </td>

  </tr>

  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/event-targets.html">Writing Event Targets</a></h4>
      Enable the objects you define to emit their own events.
    </td>

    <td>
    </td>

  </tr>

</table>

<hr>

<h2><a name="putting-it-together">Putting It Together</a></h2>

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/annotator/index.html">Annotator add-on</a></h4>
      A walkthrough of a relatively complex add-on.
    </td>

    <td>
    </td>

  </tr>

</table>

