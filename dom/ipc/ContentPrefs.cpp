/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "ContentPrefs.h"

/************************************************************
 *    DO NOT ADD PREFS TO THIS LIST WITHOUT DOM PEER REVIEW *
 ************************************************************/

const char* mozilla::dom::ContentPrefs::gInitPrefs[] = {
  "accessibility.mouse_focuses_formcontrol",
  "accessibility.tabfocus_applies_to_xul",
  "app.update.channel",
  "browser.dom.window.dump.enabled",
  "browser.sessionhistory.max_entries",
  "browser.sessionhistory.max_total_viewers",
  "device.storage.prompt.testing",
  "device.storage.writable.name",
  "dom.event.touch.coalescing.enabled",
  "dom.max_chrome_script_run_time",
  "dom.max_script_run_time",
  "dom.use_watchdog",
  "focusmanager.testmode",
  "hangmonitor.timeout",
  "javascript.enabled",
  "javascript.options.asmjs",
  "javascript.options.asyncstack",
  "javascript.options.baselinejit",
  "javascript.options.baselinejit.threshold",
  "javascript.options.baselinejit.unsafe_eager_compilation",
  "javascript.options.discardSystemSource",
  "javascript.options.dump_stack_on_debuggee_would_run",
  "javascript.options.gczeal",
  "javascript.options.gczeal.frequency",
  "javascript.options.ion",
  "javascript.options.ion.offthread_compilation",
  "javascript.options.ion.threshold",
  "javascript.options.ion.unsafe_eager_compilation",
  "javascript.options.jit.full_debug_checks",
  "javascript.options.native_regexp",
  "javascript.options.parallel_parsing",
  "javascript.options.shared_memory",
  "javascript.options.strict",
  "javascript.options.strict.debug",
  "javascript.options.throw_on_asmjs_validation_failure",
  "javascript.options.throw_on_debuggee_would_run",
  "javascript.options.wasm",
  "javascript.options.wasm_baselinejit",
  "javascript.options.werror",
  "javascript.use_us_english_locale",
  "jsloader.reuseGlobal",
  "layout.css.servo.enabled",
  "layout.css.unprefixing-service.enabled",
  "layout.css.unprefixing-service.globally-whitelisted",
  "layout.css.unprefixing-service.include-test-domains",
  "mathml.disabled",
  "media.cubeb_latency_msg_frames",
  "media.cubeb_latency_playback_ms",
  "media.decoder-doctor.wmf-disabled-is-failure",
  "media.ffvpx.low-latency.enabled",
  "media.gmp.async-shutdown-timeout",
  "media.volume_scale",
  "media.wmf.allow-unsupported-resolutions",
  "media.wmf.decoder.thread-count",
  "media.wmf.enabled",
  "media.wmf.skip-blacklist",
  "media.wmf.vp9.enabled",
  "memory.low_commit_space_threshold_mb",
  "memory.low_memory_notification_interval_ms",
  "memory.low_physical_memory_threshold_mb",
  "memory.low_virtual_mem_threshold_mb",
  "network.IDN.blacklist_chars",
  "network.IDN.restriction_profile",
  "network.IDN.use_whitelist",
  "network.IDN_show_punycode",
  "network.buffer.cache.count",
  "network.buffer.cache.size",
  "network.captive-portal-service.enabled",
  "network.dns.disablePrefetch",
  "network.dns.disablePrefetchFromHTTPS",
  "network.jar.block-remote-files",
  "network.loadinfo.skip_type_assertion",
  "network.notify.changed",
  "network.protocol-handler.external.jar",
  "network.proxy.type",
  "network.security.ports.banned",
  "network.security.ports.banned.override",
  "network.standard-url.enable-rust",
  "network.sts.max_time_for_events_between_two_polls",
  "network.sts.max_time_for_pr_close_during_shutdown",
  "network.tcp.keepalive.enabled",
  "network.tcp.keepalive.idle_time",
  "network.tcp.keepalive.probe_count",
  "network.tcp.keepalive.retry_interval",
  "network.tcp.sendbuffer",
  "security.fileuri.strict_origin_policy",
  "security.sandbox.content.level",
  "security.sandbox.content.tempDirSuffix",
  "security.sandbox.logging.enabled",
  "security.sandbox.mac.track.violations",
  "security.sandbox.windows.log",
  "security.sandbox.windows.log.stackTraceDepth",
  "shutdown.watchdog.timeoutSecs",
  "svg.disabled",
  "svg.path-caching.enabled",
  "toolkit.asyncshutdown.crash_timeout",
  "toolkit.asyncshutdown.log",
  "toolkit.osfile.log",
  "toolkit.osfile.log.redirect",
  "toolkit.telemetry.enabled",
  "toolkit.telemetry.idleTimeout",
  "toolkit.telemetry.initDelay",
  "toolkit.telemetry.log.dump",
  "toolkit.telemetry.log.level",
  "toolkit.telemetry.minSubsessionLength",
  "toolkit.telemetry.scheduler.idleTickInterval",
  "toolkit.telemetry.scheduler.tickInterval",
  "toolkit.telemetry.unified"};

const char** mozilla::dom::ContentPrefs::GetContentPrefs(size_t* aCount)
{
  *aCount = ArrayLength(ContentPrefs::gInitPrefs);
  return gInitPrefs;
}

const char*  mozilla::dom::ContentPrefs::GetContentPref(size_t aIndex)
{
  MOZ_ASSERT(aIndex < ArrayLength(ContentPrefs::gInitPrefs));
  return gInitPrefs[aIndex];
}

