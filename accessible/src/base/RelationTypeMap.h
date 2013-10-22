/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Usage: declare the macro RELATIONTYPE()with the following arguments:
 * RELATIONTYPE(geckoType, geckoTypeName, atkType, msaaType, ia2Type)
 */

RELATIONTYPE(LABELLED_BY,
             "labelled by",
             ATK_RELATION_LABELLED_BY,
             NAVRELATION_LABELLED_BY,
             IA2_RELATION_LABELLED_BY)

RELATIONTYPE(LABEL_FOR,
             "label for",
             ATK_RELATION_LABEL_FOR,
             NAVRELATION_LABEL_FOR,
             IA2_RELATION_LABEL_FOR)

RELATIONTYPE(DESCRIBED_BY,
             "described by",
             ATK_RELATION_DESCRIBED_BY,
             NAVRELATION_DESCRIBED_BY,
             IA2_RELATION_DESCRIBED_BY)

RELATIONTYPE(DESCRIPTION_FOR,
             "description for",
             ATK_RELATION_DESCRIPTION_FOR,
             NAVRELATION_DESCRIPTION_FOR,
             IA2_RELATION_DESCRIPTION_FOR)

RELATIONTYPE(NODE_CHILD_OF,
             "node child of",
             ATK_RELATION_NODE_CHILD_OF,
             NAVRELATION_NODE_CHILD_OF,
             IA2_RELATION_NODE_CHILD_OF)

RELATIONTYPE(NODE_PARENT_OF,
             "node parent of",
             ATK_RELATION_NODE_PARENT_OF,
             NAVRELATION_NODE_PARENT_OF,
             IA2_RELATION_NODE_PARENT_OF)

RELATIONTYPE(CONTROLLED_BY,
             "controlled by",
             ATK_RELATION_CONTROLLED_BY,
             NAVRELATION_CONTROLLED_BY,
             IA2_RELATION_CONTROLLED_BY)

RELATIONTYPE(CONTROLLER_FOR,
             "controller for",
             ATK_RELATION_CONTROLLER_FOR,
             NAVRELATION_CONTROLLER_FOR,
             IA2_RELATION_CONTROLLER_FOR)

RELATIONTYPE(FLOWS_TO,
             "flows to",
             ATK_RELATION_FLOWS_TO,
             NAVRELATION_FLOWS_TO,
             IA2_RELATION_FLOWS_TO)

RELATIONTYPE(FLOWS_FROM,
             "flows from",
             ATK_RELATION_FLOWS_FROM,
             NAVRELATION_FLOWS_FROM,
             IA2_RELATION_FLOWS_FROM)

RELATIONTYPE(MEMBER_OF,
             "member of",
             ATK_RELATION_MEMBER_OF,
             NAVRELATION_MEMBER_OF,
             IA2_RELATION_MEMBER_OF)

RELATIONTYPE(SUBWINDOW_OF,
             "subwindow of",
             ATK_RELATION_SUBWINDOW_OF,
             NAVRELATION_SUBWINDOW_OF,
             IA2_RELATION_SUBWINDOW_OF)

RELATIONTYPE(EMBEDS,
             "embeds",
             ATK_RELATION_EMBEDS,
             NAVRELATION_EMBEDS,
             IA2_RELATION_EMBEDS)

RELATIONTYPE(EMBEDDED_BY,
             "embedded by",
             ATK_RELATION_EMBEDDED_BY,
             NAVRELATION_EMBEDDED_BY,
             IA2_RELATION_EMBEDDED_BY)

RELATIONTYPE(POPUP_FOR,
             "popup for",
             ATK_RELATION_POPUP_FOR,
             NAVRELATION_POPUP_FOR,
             IA2_RELATION_POPUP_FOR)

RELATIONTYPE(PARENT_WINDOW_OF,
             "parent window of",
             ATK_RELATION_PARENT_WINDOW_OF,
             NAVRELATION_PARENT_WINDOW_OF,
             IA2_RELATION_PARENT_WINDOW_OF)

RELATIONTYPE(DEFAULT_BUTTON,
             "default button",
             ATK_RELATION_NULL,
             NAVRELATION_DEFAULT_BUTTON,
             IA2_RELATION_NULL)
