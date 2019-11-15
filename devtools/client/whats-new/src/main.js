/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

"use strict";

const ReactDOM = require("devtools/client/shared/vendor/react-dom");
const Types = require("devtools/client/shared/vendor/react-prop-types");
const { openDocLink } = require("devtools/client/shared/link");

const {
  Component,
  createFactory,
} = require("devtools/client/shared/vendor/react");
const dom = require("devtools/client/shared/vendor/react-dom-factories");

const aside = {
  header: "Instantly Send Tabs to Mobile",
  content: `Test your site on mobile and other devices without having to copy, paste, or leave the browser.`,
  cta: "Learn More About Send Tabs",
  href:
    "https://support.mozilla.org/en-US/kb/send-tab-firefox-desktop-other-devices?utm_source=devtools_whatsnew",
};

const release = {
  title: "What’s New in DevTools (Firefox 70 & 71)",
  linkText: "Read more",
  linkUrl:
    "https://developer.mozilla.org/docs/Mozilla/Firefox/Releases/71?utm_source=devtools&utm_medium=devtools_whatsnew",
  features: [
    {
      header: `New WebSocket Messages Inspector`,
      description: `The Network panel has a new Messages tab for inspecting WebSocket frames sent and received through the selected connection.`,
      href:
        "https://hacks.mozilla.org/2019/10/firefoxs-new-websocket-inspector/ ",
    },
    {
      header: `Block URLs in the Network panel`,
      description: `Test how a page loads without specific files, like CSS or JavaScript. Right-click network requests and select “Block URL” or use the new Request Blocking pane.`,
      href: `https://developer.mozilla.org/docs/Tools/Network_Monitor/request_list?utm_source=devtools&utm_medium=devtools_whatsnew#Block_a_specific_URL`,
    },
    {
      header: `New multi-line editor mode in Console`,
      description: `Iterate quickly on JavaScript snippets with the new multi-line editor input. It combines the ease of authoring code in an IDE with the workflow of repeatedly executing code in the context of the page.`,
      href: `https://developer.mozilla.org/docs/Tools/Web_Console/The_command_line_interpreter?utm_source=devtools&utm_medium=devtools_whatsnew`,
      hidden: true,
    },
    {
      header: `Search across all Network Headers and Content`,
      description: `The new Search pane lets you search across all network headers and response bodies in the Network panel.`,
      href: `https://developer.mozilla.org/docs/Tools/Network_Monitor/request_list?utm_source=devtools&utm_medium=devtools_whatsnew#Search_Requests`,
    },
    {
      header: `Log on Events`,
      description: `Enabling logging for Event Listener Breakpoints in the Debugger lets you observe which event handlers are being executed without the overhead of pausing.`,
      href: `https://developer.mozilla.org/docs/Tools/Debugger/Set_event_listener_breakpoints?utm_source=devtools&utm_medium=devtools_whatsnew`,
    },
    {
      header: `Quick search in Event Listeners Breakpoints`,
      description: `Quickly find the right event category and type with the new filter field in the Debugger’s Event Listener Breakpoints pane. `,
      href: `https://developer.mozilla.org/docs/Tools/Debugger/Set_event_listener_breakpoints?utm_source=devtools&utm_medium=devtools_whatsnew`,
    },
    {
      header: `New in 70: Inactive CSS rules indicator in Rules pane`,
      description: `The Inspector now grays out CSS declarations that don’t affect the selected element and shows a tooltip explaining why—and even how to fix it.`,
      href: `https://hacks.mozilla.org/2019/10/firefox-70-a-bountiful-release-for-all/#developertools`,
    },
    {
      header: `New in 70: Pause on DOM Mutation in Debugger`,
      description: `DOM Mutation Breakpoints (aka DOM Change Breakpoints) let you pause scripts that add, remove, or change specific elements.`,
      href: `https://developer.mozilla.org/docs/Tools/Debugger/Break_on_DOM_mutation?utm_source=devtools&utm_medium=devtools_whatsnew`,
    },
    {
      header: `New in 70: Color contrast information in the color picker`,
      description: `In the CSS Rules view, you can click foreground colors with the color picker to determine if their contrast with the background color meets accessibility guidelines.`,
      href: `https://developer.mozilla.org/docs/Tools/Page_Inspector/How_to/Inspect_and_select_colors?utm_source=devtools&utm_medium=devtools_whatsnew`,
    },
    {
      header: `New in 70: Auditing checks in the Accessibility inspector`,
      description: `The Accessibility Inspector’s “Check for issues” tool can now audit for keyboard accessibility in addition to color contrast and text labels.`,
      href: `https://developer.mozilla.org/docs/Tools/Accessibility_inspector?utm_source=devtools&utm_medium=devtools_whatsnew#Check_for_accessibility_issues`,
    },
  ],
};

const dev = {
  title: "Experimental Features in Firefox Developer Edition",
  linkUrl:
    "https://www.mozilla.org/firefox/developer/?utm_medium=devtools_whatsnew&utm_source=devtools",
  linkText: "Get DevEdition",
  features: [
    {
      header: `Debug Variables with Watchpoints`,
      description: `Debugger’s new Watchpoints feature lets you pause when properties get read or written. Right-click object properties in the Scopes pane when paused to use the new “Break on…” menu.`,
      href: `https://developer.mozilla.org/docs/Tools/Debugger/How_to/Set_a_watchpoint_on_a_property?utm_source=devtools&utm_medium=devtools_whatsnew`,
    },
  ],
};

class Aside extends Component {
  render() {
    return dom.aside(
      {},
      dom.div(
        { className: "call-out" },
        dom.h3({}, aside.header),
        dom.p({}, aside.content),
        dom.p(
          { className: "cta" },
          dom.a(
            {
              href: aside.href,
              className: "devtools-button",
              onClick: e => {
                e.preventDefault();
                openDocLink(aside.href);
              },
            },
            aside.cta
          )
        )
      )
    );
  }
}

class Feature extends Component {
  static get propTypes() {
    return {
      header: Types.string,
      description: Types.string,
      href: Types.string,
    };
  }

  render() {
    const { header, description, href } = this.props;
    return dom.li(
      { key: header, className: "feature" },
      dom.a(
        {
          href,
          onClick: e => {
            e.preventDefault();
            openDocLink(href);
          },
        },
        dom.h3({}, header),
        dom.p({}, description)
      )
    );
  }
}

function Link(text, href) {
  return dom.a(
    {
      className: "link",
      href,
      onClick: e => {
        e.preventDefault();
        openDocLink(href);
      },
    },
    text
  );
}

class App extends Component {
  render() {
    return dom.main(
      {},
      createFactory(Aside)(),
      dom.article(
        {},
        dom.h2(
          {},
          dom.span({}, release.title),
          Link(release.linkText, release.linkUrl)
        ),
        dom.ul(
          {},
          ...release.features
            .filter(feature => !feature.hidden)
            .map(feature => createFactory(Feature)(feature))
        ),
        dom.h2({}, dom.span({}, dev.title), Link(dev.linkText, dev.linkUrl)),
        dom.ul(
          {},
          ...dev.features.map(feature => createFactory(Feature)(feature))
        )
      )
    );
  }
}

module.exports = {
  bootstrap: () => {
    const root = document.querySelector("#root");
    ReactDOM.render(createFactory(App)(), root);
  },
  destroy: () => {
    const root = document.querySelector("#root");
    ReactDOM.unmountComponentAtNode(root);
  },
};
