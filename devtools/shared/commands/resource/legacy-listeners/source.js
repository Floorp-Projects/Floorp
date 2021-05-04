/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const ResourceCommand = require("devtools/shared/commands/resource/resource-command");

/**
 * Emit SOURCE resources, which represents a Javascript source and has the following attributes set on "available":
 *
 * - introductionType {null|String}: A string indicating how this source code was introduced into the system.
 *   This will typically be set to "scriptElement", "eval", ...
 *   But this may have many other values:
 *     https://searchfox.org/mozilla-central/rev/ac142717cc067d875e83e4b1316f004f6e063a46/dom/script/ScriptLoader.cpp#2628-2639
 *     https://searchfox.org/mozilla-central/search?q=symbol:_ZN2JS14CompileOptions19setIntroductionTypeEPKc&redirect=false
 *     https://searchfox.org/mozilla-central/rev/ac142717cc067d875e83e4b1316f004f6e063a46/devtools/server/actors/source.js#160-169
 * - sourceMapBaseURL {String}: Base URL where to look for a source map.
 *   This isn't the source map URL.
 * - sourceMapURL {null|String}: URL of the source map, if there is one.
 * - url {null|String}: URL of the source, if it relates to a particular URL.
 *   Evaled sources won't have any related URL.
 * - isBlackBoxed {Boolean}: Specifying whether the source actor's 'black-boxed' flag is set.
 * - extensionName {null|String}: If the source comes from an add-on, the add-on name.
 */
module.exports = async function({ targetCommand, targetFront, onAvailable }) {
  const isBrowserToolbox = targetCommand.targetFront.isParentProcess;
  const isNonTopLevelFrameTarget =
    !targetFront.isTopLevel &&
    targetFront.targetType === targetCommand.TYPES.FRAME;

  if (isBrowserToolbox && isNonTopLevelFrameTarget) {
    // In the BrowserToolbox, non-top-level frame targets are already
    // debugged via content-process targets.
    return;
  }

  const threadFront = await targetFront.getFront("thread");

  // Use a list of all notified SourceFront as we don't have a newSource event for all sources
  // but we sometime get sources notified both via newSource event *and* sources() method...
  // We store actor ID instead of SourceFront as it appears that multiple SourceFront for the same
  // actor are created...
  const sourcesActorIDCache = new Set();

  // Forward new sources (but also existing ones, see next comment)
  threadFront.on("newSource", ({ source }) => {
    if (sourcesActorIDCache.has(source.actor)) {
      return;
    }
    sourcesActorIDCache.add(source.actor);
    // source is a SourceActor's form, add the resourceType attribute on it
    source.resourceType = ResourceCommand.TYPES.SOURCE;
    onAvailable([source]);
  });

  // Forward already existing sources
  // Note that calling `sources()` will end up emitting `newSource` event for all existing sources.
  // But not in some cases, for example, when the thread is already paused.
  // (And yes, it means that already existing sources can be transfered twice over the wire)
  //
  // Also, browser_ext_devtools_inspectedWindow_targetSwitch.js creates many top level targets,
  // for which the SourceMapURLService will fetch sources. But these targets are destroyed while
  // the test is running and when they are, we purge all pending requests, including this one.
  // So ignore any error if this request failed on destruction.
  let sources;
  try {
    sources = await threadFront.sources();
  } catch (e) {
    if (threadFront.isDestroyed()) {
      return;
    }
    throw e;
  }

  // Note that `sources()` doesn't encapsulate SourceFront into a `source` attribute
  // while `newSource` event does.
  sources = sources.filter(source => {
    return !sourcesActorIDCache.has(source.actor);
  });
  for (const source of sources) {
    sourcesActorIDCache.add(source.actor);
    // source is a SourceActor's form, add the resourceType attribute on it
    source.resourceType = ResourceCommand.TYPES.SOURCE;
  }
  onAvailable(sources);
};
