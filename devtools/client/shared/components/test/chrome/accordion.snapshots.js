/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

window._snapshots = {
  "Accordion basic render.": {
    type: "ul",
    props: {
      className: "accordion",
      tabIndex: -1,
    },
    children: [
      {
        type: "li",
        props: {
          id: "accordion-item-1",
          className: "accordion-item",
          "aria-labelledby": "accordion-item-1-header",
        },
        children: [
          {
            type: "h2",
            props: {
              id: "accordion-item-1-header",
              className: "accordion-header",
              tabIndex: 0,
              "aria-expanded": false,
              "aria-label": "Test Accordion Item 1",
              onKeyDown: "event => this.onHeaderKeyDown(event, item)",
              onClick: "event => this.onHeaderClick(event, item)",
            },
            children: [
              {
                type: "span",
                props: {
                  className: "theme-twisty",
                  role: "presentation",
                },
                children: null,
              },
              {
                type: "span",
                props: { className: "accordion-header-label" },
                children: ["Test Accordion Item 1"],
              },
            ],
          },
          {
            type: "div",
            props: {
              className: "accordion-content",
              hidden: true,
              role: "presentation",
            },
            children: null,
          },
        ],
      },
      {
        type: "li",
        props: {
          id: "accordion-item-2",
          className: "accordion-item",
          "aria-labelledby": "accordion-item-2-header",
        },
        children: [
          {
            type: "h2",
            props: {
              id: "accordion-item-2-header",
              className: "accordion-header",
              tabIndex: 0,
              "aria-expanded": false,
              "aria-label": "Test Accordion Item 2",
              onKeyDown: "event => this.onHeaderKeyDown(event, item)",
              onClick: "event => this.onHeaderClick(event, item)",
            },
            children: [
              {
                type: "span",
                props: {
                  className: "theme-twisty",
                  role: "presentation",
                },
                children: null,
              },
              {
                type: "span",
                props: { className: "accordion-header-label" },
                children: ["Test Accordion Item 2"],
              },
              {
                type: "span",
                props: {
                  className: "accordion-header-buttons",
                  role: "presentation",
                },
                children: [
                  {
                    type: "button",
                    props: {},
                    children: null,
                  },
                ],
              },
            ],
          },
          {
            type: "div",
            props: {
              className: "accordion-content",
              hidden: true,
              role: "presentation",
            },
            children: null,
          },
        ],
      },
      {
        type: "li",
        props: {
          id: "accordion-item-3",
          className: "accordion-item accordion-open",
          "aria-labelledby": "accordion-item-3-header",
        },
        children: [
          {
            type: "h2",
            props: {
              id: "accordion-item-3-header",
              className: "accordion-header",
              tabIndex: 0,
              "aria-expanded": true,
              "aria-label": "Test Accordion Item 3",
              onKeyDown: "event => this.onHeaderKeyDown(event, item)",
              onClick: "event => this.onHeaderClick(event, item)",
            },
            children: [
              {
                type: "span",
                props: {
                  className: "theme-twisty open",
                  role: "presentation",
                },
                children: null,
              },
              {
                type: "span",
                props: {
                  className: "accordion-header-label",
                },
                children: ["Test Accordion Item 3"],
              },
            ],
          },
          {
            type: "div",
            props: {
              className: "accordion-content",
              hidden: false,
              role: "presentation",
            },
            children: [
              {
                type: "div",
                props: {},
                children: null,
              },
            ],
          },
        ],
      },
    ],
  },
};
