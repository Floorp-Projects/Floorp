/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

dictionary L10nFileSourceMockFile {
  required UTF8String path;
  required UTF8String source;
};

enum L10nFileSourceHasFileStatus {
    "present",
    "missing",
    "unknown"
};

dictionary FileSourceOptions {
  FluentBundleAddResourceOptions addResourceOptions = {};
};

/**
 * A `ResourceId` refers to a localization resource that is either required
 * or optional. The default for a `ResourceId` is that the resource is required.
 *
 * If a required resource is missing for a given locale, then the L10nRegistry
 * will not generate a bundle for that locale, because the resource is required
 * to be present in order for the bundle to be considered valid.
 *
 * If an optional resource is missing for a given locale, then the L10nRegistry
 * will still generate a bundle for that locale, but entries for the missing
 * optional resource will be missing in the bundle.
 *
 * It is recommended to only use this sparingly, since marking many resources as
 * optional will increase the state space that the L10nRegistry solver has to cover,
 * and it will have a negative impact on performance.
 *
 * We should also strive to have valid resources for all supported locales, so using
 * optional resources should be reserved for exceptional circumstances such as
 * experimental features and features under development.
 */
dictionary ResourceId {
  required UTF8String path;
  boolean _optional = false;
};

typedef (UTF8String or ResourceId) L10nResourceId;

/**
 * The interface represents a single source location for
 * the registry.
 *
 * Examples of sources are:
 *  * Toolkit omni.ja Localization Directory
 *  * Browser omni.ja Localization Directory
 *  * Language Pack Toolkit Localization Directory
 *  * Language Pack Browser Localization Directory
 */
[ChromeOnly,
 Exposed=Window]
interface L10nFileSource {
  /**
   * The `name` is the name of the `L10nFileSource`.
   *
   * The `metaSource` is the name of the package that contains the
   * `L10nFileSource`, e.g. `app` for sources packaged with the browser, or
   * `langpack` for sources packaged in an addon.
   *
   * The `locales` is a list of locales for which resources are contained in
   * the `L10nFileSource`.
   *
   * The `prePath` is the path prefix for files contained in the `L10nFileSource`.
   *
   * Optional argument `index` can be used to provide a list
   * of files available in the source.
   *
   * This is particularly useful for custom file sources which
   * provide a small number of known override files allowing the
   * registry to avoid trying I/O on the source for all
   * files not available in the source.
   */
  [Throws]
  constructor(UTF8String name, UTF8String metaSource, sequence<UTF8String> locales, UTF8String prePath, optional FileSourceOptions options = {}, optional sequence<UTF8String> index);

  /**
   * Tests may want to introduce custom file sources and
   * register them in a custom `L10nRegistry` to mock
   * behavior of using localization with test file sources.
   *
   * # Example:
   *
   * ```
   * let fs = [
   *   {
   *     "path": "/localization/en-US/test.ftl",
   *     "source": "key = Hello World!",
   *   }
   * ];
   * let mockSource = L10nFileSource.createMock("mock", "app", ["en-US"], "/localization/{locale}/", fs);
   *
   * let reg = new L10nRegistry();
   * reg.registerSources([mockSource]);
   *
   * let loc = new Localization(["test.ftl"], reg);
   * let value = await loc.formatValue("key");
   * assert(value, "Hello World!");
   * ```
   */
  [Throws]
  static L10nFileSource createMock(UTF8String name, UTF8String metasource, sequence<UTF8String> locales, UTF8String prePath, sequence<L10nFileSourceMockFile> fs);

  readonly attribute UTF8String name;
  readonly attribute UTF8String metaSource;
  [Pure, Cached, Frozen]
  readonly attribute sequence<UTF8String> locales;
  /**
   * `prePath` is the root path used together with a relative path to construct the full path used to retrieve a file
   * out of a file source.
   *
   * The `prePath` will usually contain a placeholder `{locale}` which gets replaced with a given locale.
   */
  readonly attribute UTF8String prePath;
  [Pure, Cached, Frozen]
  readonly attribute sequence<UTF8String>? index;

  [Throws]
  L10nFileSourceHasFileStatus hasFile(UTF8String locale, UTF8String path);
  [NewObject]
  Promise<FluentResource?> fetchFile(UTF8String locale, UTF8String path);
  [Throws]
  FluentResource? fetchFileSync(UTF8String locale, UTF8String path);
};

dictionary FluentBundleIteratorResult
{
  required FluentBundle? value;
  required boolean done;
};

[LegacyNoInterfaceObject, Exposed=Window]
interface FluentBundleIterator {
  FluentBundleIteratorResult next();
  [Alias="@@iterator"] FluentBundleIterator values();
};

[LegacyNoInterfaceObject, Exposed=Window]
interface FluentBundleAsyncIterator {
  [NewObject]
  Promise<FluentBundleIteratorResult> next();
  [Alias="@@asyncIterator"] FluentBundleAsyncIterator values();
};

dictionary L10nRegistryOptions {
  FluentBundleOptions bundleOptions = {};
};

[ChromeOnly, Exposed=Window]
interface L10nRegistry {
  constructor(optional L10nRegistryOptions aOptions = {});

  static L10nRegistry getInstance();

  sequence<UTF8String> getAvailableLocales();

  undefined registerSources(sequence<L10nFileSource> aSources);
  undefined updateSources(sequence<L10nFileSource> aSources);
  undefined removeSources(sequence<UTF8String> aSources);

  [Throws]
  boolean hasSource(UTF8String aName);
  [Throws]
  L10nFileSource? getSource(UTF8String aName);
  sequence<UTF8String> getSourceNames();
  undefined clearSources();

  [Throws, NewObject]
  FluentBundleIterator generateBundlesSync(sequence<UTF8String> aLocales, sequence<L10nResourceId> aResourceIds);

  [Throws, NewObject]
  FluentBundleAsyncIterator generateBundles(sequence<UTF8String> aLocales, sequence<L10nResourceId> aResourceIds);
};
