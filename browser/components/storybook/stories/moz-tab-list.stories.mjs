/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { html } from "lit.all.mjs";
// eslint-disable-next-line import/no-unassigned-import
import "browser/components/firefoxview/moz-tab-list.mjs";

const DATE_TIME_FORMATS = {
  relative: "relative",
  dateTime: "dateTime",
  date: "date",
  time: "time",
};

export default {
  title: "Domain-specific UI Widgets/Tab List",
  component: "moz-tab-list",
  argTypes: {
    dateTimeFormat: {
      options: Object.keys(DATE_TIME_FORMATS),
      mapping: DATE_TIME_FORMATS,
      control: { type: "select" },
    },
  },
};

const Template = ({
  listClass,
  dateTimeFormat,
  hasPopup,
  maxTabsLength,
  primaryAction,
  secondaryAction,
  tabItems,
}) => html`
  <style>
    main {
      max-width: 750px;
    }
    moz-tab-list.menu::part(secondary-button) {
      background-image: url("chrome://global/skin/icons/more.svg");
    }
    moz-tab-list.dismiss::part(secondary-button) {
      background-image: url("chrome://global/skin/icons/close.svg");
    }
    :host panel-item::part(button) {
      padding-inline-start: 12px;
      cursor: pointer;
    }
  </style>
  <main>
    <moz-tab-list
      class=${listClass}
      .hasPopup=${hasPopup}
      .dateTimeFormat=${dateTimeFormat}
      .maxTabsLength=${maxTabsLength}
      .tabItems=${tabItems}
      @moz-tab-list-secondary-action=${secondaryAction}
      @moz-tab-list-primary-action=${primaryAction}
    >
      <panel-list slot="menu">
        <panel-item data-l10n-id="mztabrow-delete"></panel-item>
        <panel-item data-l10n-id="mztabrow-forget-about-this-site"></panel-item>
        <hr />
        <panel-item data-l10n-id="mztabrow-open-in-window"></panel-item>
        <panel-item data-l10n-id="mztabrow-open-in-private-window"></panel-item>
        <hr />
        <panel-item data-l10n-id="mztabrow-add-bookmark"></panel-item>
        <panel-item data-l10n-id="mztabrow-save-to-pocket"></panel-item>
        <panel-item data-l10n-id="mztabrow-copy-link"></panel-item>
      </panel-list>
    </moz-tab-list>
  </main>
`;

const MAX_TABS_LENGTH = 25;

let secondaryAction = e => {
  e.target.querySelector("panel-list").toggle(e.detail.originalEvent);
};

let primaryAction = e => {
  // Open in new tab
};

const tabItems = [
  {
    icon: "chrome://global/skin/icons/defaultFavicon.svg",
    title: "Example Domain",
    url: "example.net",
    time: 1678141738136,
    primaryL10nId: "mztabrow-tabs-list-tab",
    primaryL10nArgs: JSON.stringify({ targetURI: "example.net" }),
    secondaryL10nId: "mztabrow-open-menu-button",
  },
  {
    icon: "chrome://global/skin/icons/defaultFavicon.svg",
    title: "Example Domain",
    url: "example.org",
    time: 1678141738136,
    primaryL10nId: "mztabrow-tabs-list-tab",
    primaryL10nArgs: JSON.stringify({ targetURI: "example.org" }),
    secondaryL10nId: "mztabrow-open-menu-button",
  },
  {
    icon: "chrome://global/skin/icons/defaultFavicon.svg",
    title: "Example Domain",
    url: "example.com",
    time: 1678141738136,
    primaryL10nId: "mztabrow-tabs-list-tab",
    primaryL10nArgs: JSON.stringify({ targetURI: "example.com" }),
    secondaryL10nId: "mztabrow-open-menu-button",
  },
];
const recentlyClosedItems = [
  {
    icon: "chrome://global/skin/icons/defaultFavicon.svg",
    title: "Example Domain",
    url: "example.net",
    tabid: 1,
    time: 1678141738136,
    primaryL10nId: "mztabrow-tabs-list-tab",
    primaryL10nArgs: JSON.stringify({ targetURI: "example.net" }),
    secondaryL10nId: "mztabrow-dismiss-tab-button",
    secondaryL10nArgs: JSON.stringify({
      tabTitle: "Example Domain",
    }),
  },
  {
    icon: "chrome://global/skin/icons/defaultFavicon.svg",
    title: "Example Domain",
    url: "example.org",
    tabid: 2,
    time: 1678141738136,
    primaryL10nId: "mztabrow-tabs-list-tab",
    primaryL10nArgs: JSON.stringify({ targetURI: "example.net" }),
    secondaryL10nId: "mztabrow-dismiss-tab-button",
    secondaryL10nArgs: JSON.stringify({
      tabTitle: "Example Domain",
    }),
  },
  {
    icon: "chrome://global/skin/icons/defaultFavicon.svg",
    title: "Example Domain",
    url: "example.com",
    tabid: 3,
    time: 1678141738136,
    primaryL10nId: "mztabrow-tabs-list-tab",
    primaryL10nArgs: JSON.stringify({ targetURI: "example.net" }),
    secondaryL10nId: "mztabrow-dismiss-tab-button",
    secondaryL10nArgs: JSON.stringify({
      tabTitle: "Example Domain",
    }),
  },
];

export const RelativeTime = Template.bind({});
RelativeTime.args = {
  listClass: "menu",
  dateTimeFormat: "relative",
  hasPopup: "menu",
  maxTabsLength: MAX_TABS_LENGTH,
  primaryAction,
  secondaryAction,
  tabItems,
};
export const DateAndTime = Template.bind({});
DateAndTime.args = {
  listClass: "menu",
  dateTimeFormat: "dateTime",
  maxTabsLength: MAX_TABS_LENGTH,
  primaryAction,
  secondaryAction,
  tabItems,
};
export const DateOnly = Template.bind({});
DateOnly.args = {
  listClass: "menu",
  dateTimeFormat: "date",
  hasPopup: "menu",
  maxTabsLength: MAX_TABS_LENGTH,
  primaryAction,
  secondaryAction,
  tabItems,
};
export const TimeOnly = Template.bind({});
TimeOnly.args = {
  listClass: "menu",
  dateTimeFormat: "time",
  hasPopup: "menu",
  maxTabsLength: MAX_TABS_LENGTH,
  primaryAction,
  secondaryAction,
  tabItems,
};
export const RecentlyClosed = Template.bind({});
RecentlyClosed.args = {
  listClass: "dismiss",
  dateTimeFormat: "relative",
  hasPopup: null,
  maxTabsLength: MAX_TABS_LENGTH,
  primaryAction,
  secondaryAction: () => {},
  tabItems: recentlyClosedItems,
};
