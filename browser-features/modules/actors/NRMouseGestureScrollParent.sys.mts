/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Parent actor for mouse gesture scrolling.
 * Messages sent from chrome via getActor().sendAsyncMessage() go directly
 * to the child actor's receiveMessage. This class exists for registration
 * purposes only.
 */
export class NRMouseGestureScrollParent extends JSWindowActorParent {}
