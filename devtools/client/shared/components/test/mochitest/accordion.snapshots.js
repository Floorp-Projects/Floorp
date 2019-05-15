/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

window._snapshots = {
  "Accordion basic render.": {
    "type": "ul",
    "props": {
      "className": "accordion",
      "tabIndex": -1,
    },
    "children": [
      {
        "type": "li",
        "props": {
          "className": "accordion-item-1",
          "aria-labelledby": "label-id-1",
        },
        "children": [
          {
            "type": "h2",
            "props": {
              "className": "accordion-header",
              "id": "label-id-1",
              "tabIndex": 0,
              "aria-expanded": false,
              "onKeyDown": "e => this.onHandleHeaderKeyDown(e, i)",
              "onClick": "() => this.handleHeaderClick(i)",
            },
            "children": [
              {
                "type": "div",
                "props": {
                  "className": "arrow theme-twisty",
                  "role": "presentation",
                },
                "children": null,
              },
              "Test Accordion Item 1",
            ],
          },
        ],
      },
      {
        "type": "li",
        "props": {
          "className": "accordion-item-2",
          "aria-labelledby": "label-id-2",
        },
        "children": [
          {
            "type": "h2",
            "props": {
              "className": "accordion-header",
              "id": "label-id-2",
              "tabIndex": 0,
              "aria-expanded": false,
              "onKeyDown": "e => this.onHandleHeaderKeyDown(e, i)",
              "onClick": "() => this.handleHeaderClick(i)",
            },
            "children": [
              {
                "type": "div",
                "props": {
                  "className": "arrow theme-twisty",
                  "role": "presentation",
                },
                "children": null,
              },
              "Test Accordion Item 2",
              {
                "type": "div",
                "props": {
                  "className": "header-buttons",
                  "role": "presentation",
                },
                "children": [
                  {
                    "type": "button",
                    "props": {},
                    "children": null,
                  },
                ],
              },
            ],
          },
        ],
      },
      {
        "type": "li",
        "props": {
          "className": "accordion-item-3",
          "aria-labelledby": "label-id-3",
        },
        "children": [
          {
            "type": "h2",
            "props": {
              "className": "accordion-header",
              "id": "label-id-3",
              "tabIndex": 0,
              "aria-expanded": true,
              "onKeyDown": "e => this.onHandleHeaderKeyDown(e, i)",
              "onClick": "() => this.handleHeaderClick(i)",
            },
            "children": [
              {
                "type": "div",
                "props": {
                  "className": "arrow theme-twisty open",
                  "role": "presentation",
                },
                "children": null,
              },
              "Test Accordion Item 3",
            ],
          },
          {
            "type": "div",
            "props": {
              "className": "accordion-content",
              "role": "presentation",
            },
            "children": [
              {
                "type": "div",
                "props": {},
                "children": null,
              },
            ],
          },
        ],
      },
    ],
  },
};
