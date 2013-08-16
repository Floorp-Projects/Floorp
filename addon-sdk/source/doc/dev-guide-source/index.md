<!-- This Source Code Form is subject to the terms of the Mozilla Public
   - License, v. 2.0. If a copy of the MPL was not distributed with this
   - file, You can obtain one at http://mozilla.org/MPL/2.0/. -->

<h2 class="top">Welcome to the Add-on SDK!</h2>

Using the Add-on SDK you can create Firefox add-ons using standard Web
technologies: JavaScript, HTML, and CSS. The SDK includes JavaScript APIs which you can use to create add-ons, and tools for creating, running, testing, and packaging add-ons.

<hr>

## <a href="dev-guide/tutorials/index.html">Tutorials</a> ##

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/index.html#getting-started">Getting started</a></h4>
      How to
      <a href="dev-guide/tutorials/installation.html">install the SDK</a> and
      <a href="dev-guide/tutorials/getting-started-with-cfx.html">use the cfx
      tool</a> to develop, test, and package add-ons.
    </td>

    <td>
      <h4><a href="dev-guide/tutorials/index.html#create-user-interfaces">Create user interface components</a></h4>
      Create user interface components such as
        <a href="dev-guide/tutorials/adding-toolbar-button.html">toolbar buttons</a>,
        <a href="dev-guide/tutorials/adding-menus.html">menu items</a>, and
        <a href="dev-guide/tutorials/display-a-popup.html">dialogs</a>
    </td>
  </tr>

  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/index.html#interact-with-the-browser">Interact with the browser</a></h4>
      <a href="dev-guide/tutorials/open-a-web-page.html">Open web pages</a>,
      <a href="dev-guide/tutorials/listen-for-page-load.html">listen for pages loading</a>, and
      <a href="dev-guide/tutorials/list-open-tabs.html">list open pages</a>.
    </td>

    <td>
      <h4><a href="dev-guide/tutorials/index.html#modify-web-pages">Modify web pages</a></h4>
      <a href="dev-guide/tutorials/modifying-web-pages-url.html">Modify pages matching a URL pattern</a>
      or <a href="dev-guide/tutorials/modifying-web-pages-tab.html">dynamically modify a particular tab</a>.
    </td>
  </tr>

  <tr>
    <td>
      <h4><a href="dev-guide/tutorials/index.html#development-techniques">Development techniques</a></h4>
Learn about common development techniques, such as
<a href="dev-guide/tutorials/unit-testing.html">unit testing</a>,
<a href="dev-guide/tutorials/logging.html">logging</a>,
<a href="dev-guide/tutorials/reusable-modules.html">creating reusable modules</a>,
<a href="dev-guide/tutorials/l10n.html">localization</a>, and
<a href="dev-guide/tutorials/mobile.html">mobile development</a>.
    </td>

    <td>
      <h4><a href="dev-guide/tutorials/index.html#putting-it-together">Putting it together</a></h4>
      Walkthrough of the <a href="dev-guide/tutorials/annotator/index.html">Annotator</a> example add-on.
    </td>
  </tr>

</table>

<hr>

## <a href="dev-guide/guides/index.html">Guides</a> ##

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="dev-guide/guides/index.html#contributors-guide">Contributor's Guide</a></h4>
      Learn
      <a href="dev-guide/guides/contributors-guide/getting-started.html">how to start contributing</a> to the SDK,
      and about the most important idioms used in the SDK code, such as
      <a href="dev-guide/guides/contributors-guide/modules.html">modules</a>,
      <a href="dev-guide/guides/contributors-guide/classes-and-inheritance.html">classes and inheritance</a>,
      <a href="dev-guide/guides/contributors-guide/private-properties.html">private properties</a>, and
      <a href="dev-guide/guides/contributors-guide/content-processes.html">content processes</a>.
    </td>

    <td>
      <h4><a href="dev-guide/guides/index.html#sdk-idioms">SDK idioms</a></h4>
      The SDK's
      <a href="dev-guide/guides/events.html">event framework</a> and the
      <a href="dev-guide/guides/two-types-of-scripts.html">distinction between add-on scripts and content scripts</a>.
    </td>

  </tr>

  <tr>
    <td>
      <h4><a href="dev-guide/guides/index.html#sdk-infrastructure">SDK infrastructure</a></h4>
      Aspects of the SDK's underlying technology:
      <a href="dev-guide/guides/modules.html">Modules</a>, the
      <a href="dev-guide/guides/program-id.html">Program ID</a>,
      and the rules defining
      <a href="dev-guide/guides/firefox-compatibility.html">Firefox compatibility</a>.
    </td>

    <td>
      <h4><a href="dev-guide/guides/index.html#xul-migration">XUL migration</a></h4>
      A guide to <a href="dev-guide/guides/xul-migration.html">porting XUL add-ons to the SDK</a>.
      This guide includes a
      <a href="dev-guide/guides/sdk-vs-xul.html">comparison of the two toolsets</a> and a
      <a href="dev-guide/guides/library-detector.html">worked example</a> of porting a XUL add-on.
    </td>

  </tr>

  <tr>
    <td>
      <h4><a href="dev-guide/guides/index.html#content-scripts">Content scripts</a></h4>
      A <a href="dev-guide/guides/content-scripts/index.html">detailed guide to working with content scripts</a>,
      including: how to load content scripts, which objects
      content scripts can access, and how to communicate
      between content scripts and the rest of your add-on.
    </td>

    <td>
    </td>

  </tr>

</table>

<hr>

## Reference ##

<table class="catalog">
<colgroup>
<col width="50%">
<col width="50%">
</colgroup>
  <tr>
    <td>
      <h4><a href="modules/high-level-modules.html">High-Level APIs</a></h4>
      Reference documentation for the high-level SDK APIs.
    </td>

    <td>
      <h4><a href="modules/low-level-modules.html">Low-Level APIs</a></h4>
      Reference documentation for the low-level SDK APIs.
    </td>
  </tr>

  <tr>
    <td>
      <h4>Tools reference</h4>
      Reference documentation for the
      <a href="dev-guide/cfx-tool.html">cfx tool</a>
      used to develop, test, and package add-ons, the
      <a href="dev-guide/console.html">console</a>
      global used for logging, and the
      <a href="dev-guide/package-spec.html">package.json</a> file.
    </td>
    <td>
    </td>
  </tr>

</table>
