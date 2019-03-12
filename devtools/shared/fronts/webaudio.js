/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  audionodeSpec,
  webAudioSpec,
  AUTOMATION_METHODS,
  NODE_CREATION_METHODS,
  NODE_ROUTING_METHODS,
} = require("devtools/shared/specs/webaudio");
const { FrontClassWithSpec, registerFront } = require("devtools/shared/protocol");
loader.lazyRequireGetter(this, "AUDIO_NODE_DEFINITION",
  "devtools/server/actors/utils/audionodes.json");

/**
 * The corresponding Front object for the AudioNodeActor.
 *
 * @attribute {String} type
 *            The type of audio node, like "OscillatorNode", "MediaElementAudioSourceNode"
 * @attribute {Boolean} source
 *            Boolean indicating if the node is a source node, like BufferSourceNode,
 *            MediaElementAudioSourceNode, OscillatorNode, etc.
 * @attribute {Boolean} bypassable
 *            Boolean indicating if the audio node is bypassable (splitter,
 *            merger and destination nodes, for example, are not)
 */
class AudioNodeFront extends FrontClassWithSpec(audionodeSpec) {
  form(form) {
    this.actorID = form.actor;
    this.type = form.type;
    this.source = form.source;
    this.bypassable = form.bypassable;
  }
}

exports.AudioNodeFront = AudioNodeFront;
registerFront(AudioNodeFront);

/**
 * The corresponding Front object for the WebAudioActor.
 */
class WebAudioFront extends FrontClassWithSpec(webAudioSpec) {
  constructor(client) {
    super(client);

    this.before("create-node", this._onCreateNode.bind(this));

    // Attribute name from which to retrieve the actorID out of the target actor's form
    this.formAttributeName = "webaudioActor";
  }

  /**
   * If connecting to older geckos (<Fx43), where audio node actor's do not
   * contain `type`, `source` and `bypassable` properties, fetch
   * them manually here.
   */
  _onCreateNode(audionode) {
    if (!audionode.type) {
      return audionode.getType().then(type => {
        audionode.type = type;
        audionode.source = !!AUDIO_NODE_DEFINITION[type].source;
        audionode.bypassable = !AUDIO_NODE_DEFINITION[type].unbypassable;
      });
    }
    return null;
  }
}

WebAudioFront.AUTOMATION_METHODS = new Set(AUTOMATION_METHODS);
WebAudioFront.NODE_CREATION_METHODS = new Set(NODE_CREATION_METHODS);
WebAudioFront.NODE_ROUTING_METHODS = new Set(NODE_ROUTING_METHODS);

exports.WebAudioFront = WebAudioFront;
registerFront(WebAudioFront);
