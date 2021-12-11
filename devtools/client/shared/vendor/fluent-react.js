/* Copyright 2019 Mozilla Foundation and others
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */


'use strict';

/* fluent-react@0.10.0 */

Object.defineProperty(exports, '__esModule', { value: true });

function _interopDefault (ex) { return (ex && (typeof ex === 'object') && 'default' in ex) ? ex['default'] : ex; }

const react = require('devtools/client/shared/vendor/react');
const PropTypes = _interopDefault(require('devtools/client/shared/vendor/react-prop-types'));

/*
 * Synchronously map an identifier or an array of identifiers to the best
 * `FluentBundle` instance(s).
 *
 * @param {Iterable} iterable
 * @param {string|Array<string>} ids
 * @returns {FluentBundle|Array<FluentBundle>}
 */
function mapBundleSync(iterable, ids) {
  if (!Array.isArray(ids)) {
    return getBundleForId(iterable, ids);
  }

  return ids.map(
    id => getBundleForId(iterable, id)
  );
}

/*
 * Find the best `FluentBundle` with the translation for `id`.
 */
function getBundleForId(iterable, id) {
  for (const bundle of iterable) {
    if (bundle.hasMessage(id)) {
      return bundle;
    }
  }

  return null;
}

/*
 * Asynchronously map an identifier or an array of identifiers to the best
 * `FluentBundle` instance(s).
 *
 * @param {AsyncIterable} iterable
 * @param {string|Array<string>} ids
 * @returns {Promise<FluentBundle|Array<FluentBundle>>}
 */

/*
 * @module fluent-sequence
 * @overview Manage ordered sequences of FluentBundles.
 */

/*
 * Base CachedIterable class.
 */
class CachedIterable extends Array {
    /**
     * Create a `CachedIterable` instance from an iterable or, if another
     * instance of `CachedIterable` is passed, return it without any
     * modifications.
     *
     * @param {Iterable} iterable
     * @returns {CachedIterable}
     */
    static from(iterable) {
        if (iterable instanceof this) {
            return iterable;
        }

        return new this(iterable);
    }
}

/*
 * CachedSyncIterable caches the elements yielded by an iterable.
 *
 * It can be used to iterate over an iterable many times without depleting the
 * iterable.
 */
class CachedSyncIterable extends CachedIterable {
    /**
     * Create an `CachedSyncIterable` instance.
     *
     * @param {Iterable} iterable
     * @returns {CachedSyncIterable}
     */
    constructor(iterable) {
        super();

        if (Symbol.iterator in Object(iterable)) {
            this.iterator = iterable[Symbol.iterator]();
        } else {
            throw new TypeError("Argument must implement the iteration protocol.");
        }
    }

    [Symbol.iterator]() {
        const cached = this;
        let cur = 0;

        return {
            next() {
                if (cached.length <= cur) {
                    cached.push(cached.iterator.next());
                }
                return cached[cur++];
            }
        };
    }

    /**
     * This method allows user to consume the next element from the iterator
     * into the cache.
     *
     * @param {number} count - number of elements to consume
     */
    touchNext(count = 1) {
        let idx = 0;
        while (idx++ < count) {
            const last = this[this.length - 1];
            if (last && last.done) {
                break;
            }
            this.push(this.iterator.next());
        }
        // Return the last cached {value, done} object to allow the calling
        // code to decide if it needs to call touchNext again.
        return this[this.length - 1];
    }
}

/*
 * `ReactLocalization` handles translation formatting and fallback.
 *
 * The current negotiated fallback chain of languages is stored in the
 * `ReactLocalization` instance in form of an iterable of `FluentBundle`
 * instances.  This iterable is used to find the best existing translation for
 * a given identifier.
 *
 * `Localized` components must subscribe to the changes of the
 * `ReactLocalization`'s fallback chain.  When the fallback chain changes (the
 * `bundles` iterable is set anew), all subscribed compontent must relocalize.
 *
 * The `ReactLocalization` class instances are exposed to `Localized` elements
 * via the `LocalizationProvider` component.
 */
class ReactLocalization {
  constructor(bundles) {
    this.bundles = CachedSyncIterable.from(bundles);
    this.subs = new Set();
  }

  /*
   * Subscribe a `Localized` component to changes of `bundles`.
   */
  subscribe(comp) {
    this.subs.add(comp);
  }

  /*
   * Unsubscribe a `Localized` component from `bundles` changes.
   */
  unsubscribe(comp) {
    this.subs.delete(comp);
  }

  /*
   * Set a new `bundles` iterable and trigger the retranslation.
   */
  setBundles(bundles) {
    this.bundles = CachedSyncIterable.from(bundles);

    // Update all subscribed Localized components.
    this.subs.forEach(comp => comp.relocalize());
  }

  getBundle(id) {
    return mapBundleSync(this.bundles, id);
  }

  /*
   * Find a translation by `id` and format it to a string using `args`.
   */
  getString(id, args, fallback) {
    const bundle = this.getBundle(id);
    if (bundle) {
      const msg = bundle.getMessage(id);
      if (msg && msg.value) {
        let errors = [];
        let value = bundle.formatPattern(msg.value, args, errors);
        for (let error of errors) {
          this.reportError(error);
        }
        return value;
      }
    }

    return fallback || id;
  }

  // XXX Control this via a prop passed to the LocalizationProvider.
  // See https://github.com/projectfluent/fluent.js/issues/411.
  reportError(error) {
    /* global console */
    // eslint-disable-next-line no-console
    console.warn(`[@fluent/react] ${error.name}: ${error.message}`);
  }
}

function isReactLocalization(props, propName) {
  const prop = props[propName];

  if (prop instanceof ReactLocalization) {
    return null;
  }

  return new Error(
    `The ${propName} context field must be an instance of ReactLocalization.`
  );
}

/* eslint-env browser */

let cachedParseMarkup;

// We use a function creator to make the reference to `document` lazy. At the
// same time, it's eager enough to throw in <LocalizationProvider> as soon as
// it's first mounted which reduces the risk of this error making it to the
// runtime without developers noticing it in development.
function createParseMarkup() {
  if (typeof(document) === "undefined") {
    // We can't use <template> to sanitize translations.
    throw new Error(
      "`document` is undefined. Without it, translations cannot " +
      "be safely sanitized. Consult the documentation at " +
      "https://github.com/projectfluent/fluent.js/wiki/React-Overlays."
    );
  }

  if (!cachedParseMarkup) {
    const template = document.createElement("template");
    cachedParseMarkup = function parseMarkup(str) {
      template.innerHTML = str;
      return Array.from(template.content.childNodes);
    };
  }

  return cachedParseMarkup;
}

/*
 * The Provider component for the `ReactLocalization` class.
 *
 * Exposes a `ReactLocalization` instance to all descendants via React's
 * context feature.  It makes translations available to all localizable
 * elements in the descendant's render tree without the need to pass them
 * explicitly.
 *
 *     <LocalizationProvider bundles={…}>
 *         …
 *     </LocalizationProvider>
 *
 * The `LocalizationProvider` component takes one prop: `bundles`.  It should
 * be an iterable of `FluentBundle` instances in order of the user's
 * preferred languages.  The `FluentBundle` instances will be used by
 * `ReactLocalization` to format translations.  If a translation is missing in
 * one instance, `ReactLocalization` will fall back to the next one.
 */
class LocalizationProvider extends react.Component {
  constructor(props) {
    super(props);
    const {bundles, parseMarkup} = props;

    if (bundles === undefined) {
      throw new Error("LocalizationProvider must receive the bundles prop.");
    }

    if (!bundles[Symbol.iterator]) {
      throw new Error("The bundles prop must be an iterable.");
    }

    this.l10n = new ReactLocalization(bundles);
    this.parseMarkup = parseMarkup || createParseMarkup();
  }

  getChildContext() {
    return {
      l10n: this.l10n,
      parseMarkup: this.parseMarkup,
    };
  }

  componentWillReceiveProps(next) {
    const { bundles } = next;

    if (bundles !== this.props.bundles) {
      this.l10n.setBundles(bundles);
    }
  }

  render() {
    return react.Children.only(this.props.children);
  }
}

LocalizationProvider.childContextTypes = {
  l10n: isReactLocalization,
  parseMarkup: PropTypes.func,
};

LocalizationProvider.propTypes = {
  children: PropTypes.element.isRequired,
  bundles: isIterable,
  parseMarkup: PropTypes.func,
};

function isIterable(props, propName, componentName) {
  const prop = props[propName];

  if (Symbol.iterator in Object(prop)) {
    return null;
  }

  return new Error(
    `The ${propName} prop supplied to ${componentName} must be an iterable.`
  );
}

function withLocalization(Inner) {
  class WithLocalization extends react.Component {
    componentDidMount() {
      const { l10n } = this.context;

      if (l10n) {
        l10n.subscribe(this);
      }
    }

    componentWillUnmount() {
      const { l10n } = this.context;

      if (l10n) {
        l10n.unsubscribe(this);
      }
    }

    /*
     * Rerender this component in a new language.
     */
    relocalize() {
      // When the `ReactLocalization`'s fallback chain changes, update the
      // component.
      this.forceUpdate();
    }

    /*
     * Find a translation by `id` and format it to a string using `args`.
     */
    getString(id, args, fallback) {
      const { l10n } = this.context;

      if (!l10n) {
        return fallback || id;
      }

      return l10n.getString(id, args, fallback);
    }

    render() {
      return react.createElement(
        Inner,
        Object.assign(
          // getString needs to be re-bound on updates to trigger a re-render
          { getString: (...args) => this.getString(...args) },
          this.props
        )
      );
    }
  }

  WithLocalization.displayName = `WithLocalization(${displayName(Inner)})`;

  WithLocalization.contextTypes = {
    l10n: isReactLocalization
  };

  return WithLocalization;
}

function displayName(component) {
  return component.displayName || component.name || "Component";
}

/**
 * Copyright (c) 2013-present, Facebook, Inc.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in this directory.
 */

// For HTML, certain tags should omit their close tag. We keep a whitelist for
// those special-case tags.

var omittedCloseTags = {
  area: true,
  base: true,
  br: true,
  col: true,
  embed: true,
  hr: true,
  img: true,
  input: true,
  keygen: true,
  link: true,
  meta: true,
  param: true,
  source: true,
  track: true,
  wbr: true,
  // NOTE: menuitem's close tag should be omitted, but that causes problems.
};

/**
 * Copyright (c) 2013-present, Facebook, Inc.
 *
 * This source code is licensed under the MIT license found in the
 * LICENSE file in this directory.
 */

// For HTML, certain tags cannot have children. This has the same purpose as
// `omittedCloseTags` except that `menuitem` should still have its closing tag.

var voidElementTags = {
  menuitem: true,
  ...omittedCloseTags,
};

// Match the opening angle bracket (<) in HTML tags, and HTML entities like
// &amp;, &#0038;, &#x0026;.
const reMarkup = /<|&#?\w+;/;

/*
 * Prepare props passed to `Localized` for formatting.
 */
function toArguments(props) {
  const args = {};
  const elems = {};

  for (const [propname, propval] of Object.entries(props)) {
    if (propname.startsWith("$")) {
      const name = propname.substr(1);
      args[name] = propval;
    } else if (react.isValidElement(propval)) {
      // We'll try to match localNames of elements found in the translation with
      // names of elements passed as props. localNames are always lowercase.
      const name = propname.toLowerCase();
      elems[name] = propval;
    }
  }

  return [args, elems];
}

/*
 * The `Localized` class renders its child with translated props and children.
 *
 *     <Localized id="hello-world">
 *         <p>{'Hello, world!'}</p>
 *     </Localized>
 *
 * The `id` prop should be the unique identifier of the translation.  Any
 * attributes found in the translation will be applied to the wrapped element.
 *
 * Arguments to the translation can be passed as `$`-prefixed props on
 * `Localized`.
 *
 *     <Localized id="hello-world" $username={name}>
 *         <p>{'Hello, { $username }!'}</p>
 *     </Localized>
 *
 *  It's recommended that the contents of the wrapped component be a string
 *  expression.  The string will be used as the ultimate fallback if no
 *  translation is available.  It also makes it easy to grep for strings in the
 *  source code.
 */
class Localized extends react.Component {
  componentDidMount() {
    const { l10n } = this.context;

    if (l10n) {
      l10n.subscribe(this);
    }
  }

  componentWillUnmount() {
    const { l10n } = this.context;

    if (l10n) {
      l10n.unsubscribe(this);
    }
  }

  /*
   * Rerender this component in a new language.
   */
  relocalize() {
    // When the `ReactLocalization`'s fallback chain changes, update the
    // component.
    this.forceUpdate();
  }

  render() {
    const { l10n, parseMarkup } = this.context;
    const { id, attrs, children: child = null } = this.props;

    // Validate that the child element isn't an array
    if (Array.isArray(child)) {
      throw new Error("<Localized/> expected to receive a single " +
        "React node child");
    }

    if (!l10n) {
      // Use the wrapped component as fallback.
      return child;
    }

    const bundle = l10n.getBundle(id);

    if (bundle === null) {
      // Use the wrapped component as fallback.
      return child;
    }

    const msg = bundle.getMessage(id);
    const [args, elems] = toArguments(this.props);
    let errors = [];

    // Check if the child inside <Localized> is a valid element -- if not, then
    // it's either null or a simple fallback string. No need to localize the
    // attributes.
    if (!react.isValidElement(child)) {
      if (msg.value) {
        // Replace the fallback string with the message value;
        let value = bundle.formatPattern(msg.value, args, errors);
        for (let error of errors) {
          l10n.reportError(error);
        }
        return value;
      }

      return child;
    }

    let localizedProps;

    // The default is to forbid all message attributes. If the attrs prop exists
    // on the Localized instance, only set message attributes which have been
    // explicitly allowed by the developer.
    if (attrs && msg.attributes) {
      localizedProps = {};
      errors = [];
      for (const [name, allowed] of Object.entries(attrs)) {
        if (allowed && name in msg.attributes) {
          localizedProps[name] = bundle.formatPattern(
            msg.attributes[name], args, errors);
        }
      }
      for (let error of errors) {
        l10n.reportError(error);
      }
    }

    // If the wrapped component is a known void element, explicitly dismiss the
    // message value and do not pass it to cloneElement in order to avoid the
    // "void element tags must neither have `children` nor use
    // `dangerouslySetInnerHTML`" error.
    if (child.type in voidElementTags) {
      return react.cloneElement(child, localizedProps);
    }

    // If the message has a null value, we're only interested in its attributes.
    // Do not pass the null value to cloneElement as it would nuke all children
    // of the wrapped component.
    if (msg.value === null) {
      return react.cloneElement(child, localizedProps);
    }

    errors = [];
    const messageValue = bundle.formatPattern(msg.value, args, errors);
    for (let error of errors) {
      l10n.reportError(error);
    }

    // If the message value doesn't contain any markup nor any HTML entities,
    // insert it as the only child of the wrapped component.
    if (!reMarkup.test(messageValue)) {
      return react.cloneElement(child, localizedProps, messageValue);
    }

    // If the message contains markup, parse it and try to match the children
    // found in the translation with the props passed to this Localized.
    const translationNodes = parseMarkup(messageValue);
    const translatedChildren = translationNodes.map(childNode => {
      if (childNode.nodeType === childNode.TEXT_NODE) {
        return childNode.textContent;
      }

      // If the child is not expected just take its textContent.
      if (!elems.hasOwnProperty(childNode.localName)) {
        return childNode.textContent;
      }

      const sourceChild = elems[childNode.localName];

      // If the element passed as a prop to <Localized> is a known void element,
      // explicitly dismiss any textContent which might have accidentally been
      // defined in the translation to prevent the "void element tags must not
      // have children" error.
      if (sourceChild.type in voidElementTags) {
        return sourceChild;
      }

      // TODO Protect contents of elements wrapped in <Localized>
      // https://github.com/projectfluent/fluent.js/issues/184
      // TODO  Control localizable attributes on elements passed as props
      // https://github.com/projectfluent/fluent.js/issues/185
      return react.cloneElement(sourceChild, null, childNode.textContent);
    });

    return react.cloneElement(child, localizedProps, ...translatedChildren);
  }
}

Localized.contextTypes = {
  l10n: isReactLocalization,
  parseMarkup: PropTypes.func,
};

Localized.propTypes = {
  children: PropTypes.node
};

/*
 * @module fluent-react
 * @overview
 *

 * `fluent-react` provides React bindings for Fluent.  It takes advantage of
 * React's Components system and the virtual DOM.  Translations are exposed to
 * components via the provider pattern.
 *
 *     <LocalizationProvider bundles={…}>
 *         <Localized id="hello-world">
 *             <p>{'Hello, world!'}</p>
 *         </Localized>
 *     </LocalizationProvider>
 *
 * Consult the documentation of the `LocalizationProvider` and the `Localized`
 * components for more information.
 */

exports.LocalizationProvider = LocalizationProvider;
exports.Localized = Localized;
exports.ReactLocalization = ReactLocalization;
exports.isReactLocalization = isReactLocalization;
exports.withLocalization = withLocalization;
