# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

<!-- Applications panel -->

<script type="application/javascript"
        src="chrome://browser/content/preferences/in-content/applications.js"/>

<preferences id="feedsPreferences" hidden="true" data-category="paneApplications">
  <preference id="browser.feeds.handler"
              name="browser.feeds.handler"
              type="string"/>
  <preference id="browser.feeds.handler.default"
              name="browser.feeds.handler.default"
              type="string"/>
  <preference id="browser.feeds.handlers.application"
              name="browser.feeds.handlers.application"
              type="file"/>
  <preference id="browser.feeds.handlers.webservice"
              name="browser.feeds.handlers.webservice"
              type="string"/>

  <preference id="browser.videoFeeds.handler"
              name="browser.videoFeeds.handler"
              type="string"/>
  <preference id="browser.videoFeeds.handler.default"
              name="browser.videoFeeds.handler.default"
              type="string"/>
  <preference id="browser.videoFeeds.handlers.application"
              name="browser.videoFeeds.handlers.application"
              type="file"/>
  <preference id="browser.videoFeeds.handlers.webservice"
              name="browser.videoFeeds.handlers.webservice"
              type="string"/>

  <preference id="browser.audioFeeds.handler"
              name="browser.audioFeeds.handler"
              type="string"/>
  <preference id="browser.audioFeeds.handler.default"
              name="browser.audioFeeds.handler.default"
              type="string"/>
  <preference id="browser.audioFeeds.handlers.application"
              name="browser.audioFeeds.handlers.application"
              type="file"/>
  <preference id="browser.audioFeeds.handlers.webservice"
              name="browser.audioFeeds.handlers.webservice"
              type="string"/>

  <preference id="pref.downloads.disable_button.edit_actions"
              name="pref.downloads.disable_button.edit_actions"
              type="bool"/>
</preferences>

<keyset data-category="paneApplications">
  <!-- Ctrl+f/k focus the search box in the Applications pane.
       These <key>s have oncommand attributes because of bug 371900. -->
  <key key="&focusSearch1.key;" modifiers="accel" id="focusSearch1" oncommand=";"/>
  <key key="&focusSearch2.key;" modifiers="accel" id="focusSearch2" oncommand=";"/>
</keyset>

<hbox id="header-applications"
      class="header"
      hidden="true"
      data-category="paneApplications">
  <label class="header-name" flex="1">&paneApplications.title;</label>
  <html:a class="help-button" target="_blank" aria-label="&helpButton.label;"></html:a>
</hbox>

<vbox id="applicationsContent"
      data-category="paneApplications"
      hidden="true"
      flex="1">
  <hbox>
    <textbox id="filter" flex="1"
             type="search"
             placeholder="&filter.emptytext;"
             aria-controls="handlersView"/>
  </hbox>

  <separator class="thin"/>

  <richlistbox id="handlersView" orient="vertical" persist="lastSelectedType"
               preference="pref.downloads.disable_button.edit_actions"
               flex="1">
    <listheader equalsize="always">
        <treecol id="typeColumn" label="&typeColumn.label;" value="type"
                 accesskey="&typeColumn.accesskey;" persist="sortDirection"
                 flex="1" sortDirection="ascending"/>
        <treecol id="actionColumn" label="&actionColumn2.label;" value="action"
                 accesskey="&actionColumn2.accesskey;" persist="sortDirection"
                 flex="1"/>
    </listheader>
  </richlistbox>
</vbox>
