/* fluent-react@0.7.0 */
(function (global, factory) {
  typeof exports === 'object' && typeof module !== 'undefined' ? factory(exports, require('devtools/client/shared/vendor/fluent'), require('devtools/client/shared/vendor/react'), require('devtools/client/shared/vendor/react-prop-types')) :
  typeof define === 'function' && define.amd ? define('fluent-react', ['exports', 'fluent', 'react', 'prop-types'], factory) :
  (factory((global.FluentReact = {}),global.Fluent,global.React,global.PropTypes));
}(this, (function (exports,fluent,react,PropTypes) { 'use strict';

  PropTypes = PropTypes && PropTypes.hasOwnProperty('default') ? PropTypes['default'] : PropTypes;

  /*
   * `ReactLocalization` handles translation formatting and fallback.
   *
   * The current negotiated fallback chain of languages is stored in the
   * `ReactLocalization` instance in form of an iterable of `MessageContext`
   * instances.  This iterable is used to find the best existing translation for
   * a given identifier.
   *
   * `Localized` components must subscribe to the changes of the
   * `ReactLocalization`'s fallback chain.  When the fallback chain changes (the
   * `messages` iterable is set anew), all subscribed compontent must relocalize.
   *
   * The `ReactLocalization` class instances are exposed to `Localized` elements
   * via the `LocalizationProvider` component.
   */
  class ReactLocalization {
    constructor(messages) {
      this.contexts = new fluent.CachedIterable(messages);
      this.subs = new Set();
    }

    /*
     * Subscribe a `Localized` component to changes of `messages`.
     */
    subscribe(comp) {
      this.subs.add(comp);
    }

    /*
     * Unsubscribe a `Localized` component from `messages` changes.
     */
    unsubscribe(comp) {
      this.subs.delete(comp);
    }

    /*
     * Set a new `messages` iterable and trigger the retranslation.
     */
    setMessages(messages) {
      this.contexts = new fluent.CachedIterable(messages);

      // Update all subscribed Localized components.
      this.subs.forEach(comp => comp.relocalize());
    }

    getMessageContext(id) {
      return fluent.mapContextSync(this.contexts, id);
    }

    formatCompound(mcx, msg, args) {
      const value = mcx.format(msg, args);

      if (msg.attrs) {
        var attrs = {};
        for (const name of Object.keys(msg.attrs)) {
          attrs[name] = mcx.format(msg.attrs[name], args);
        }
      }

      return { value, attrs };
    }

    /*
     * Find a translation by `id` and format it to a string using `args`.
     */
    getString(id, args, fallback) {
      const mcx = this.getMessageContext(id);

      if (mcx === null) {
        return fallback || id;
      }

      const msg = mcx.getMessage(id);
      return mcx.format(msg, args);
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

  /*
   * The Provider component for the `ReactLocalization` class.
   *
   * Exposes a `ReactLocalization` instance to all descendants via React's
   * context feature.  It makes translations available to all localizable
   * elements in the descendant's render tree without the need to pass them
   * explicitly.
   *
   *     <LocalizationProvider messages={…}>
   *         …
   *     </LocalizationProvider>
   *
   * The `LocalizationProvider` component takes one prop: `messages`.  It should
   * be an iterable of `MessageContext` instances in order of the user's
   * preferred languages.  The `MessageContext` instances will be used by
   * `ReactLocalization` to format translations.  If a translation is missing in
   * one instance, `ReactLocalization` will fall back to the next one.
   */
  class LocalizationProvider extends react.Component {
    constructor(props) {
      super(props);
      const { messages } = props;

      if (messages === undefined) {
        throw new Error("LocalizationProvider must receive the messages prop.");
      }

      if (!messages[Symbol.iterator]) {
        throw new Error("The messages prop must be an iterable.");
      }

      this.l10n = new ReactLocalization(messages);
    }

    getChildContext() {
      return {
        l10n: this.l10n
      };
    }

    componentWillReceiveProps(next) {
      const { messages } = next;

      if (messages !== this.props.messages) {
        this.l10n.setMessages(messages);
      }
    }

    render() {
      return react.Children.only(this.props.children);
    }
  }

  LocalizationProvider.childContextTypes = {
    l10n: isReactLocalization
  };

  LocalizationProvider.propTypes = {
    children: PropTypes.element.isRequired,
    messages: isIterable
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

  /* eslint-env browser */

  const TEMPLATE = document.createElement("template");

  function parseMarkup(str) {
    TEMPLATE.innerHTML = str;
    return TEMPLATE.content;
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
      const { l10n } = this.context;
      const { id, attrs, children } = this.props;
      const elem = react.Children.only(children);

      if (!l10n) {
        // Use the wrapped component as fallback.
        return elem;
      }

      const mcx = l10n.getMessageContext(id);

      if (mcx === null) {
        // Use the wrapped component as fallback.
        return elem;
      }

      const msg = mcx.getMessage(id);
      const [args, elems] = toArguments(this.props);
      const {
        value: messageValue,
        attrs: messageAttrs
      } = l10n.formatCompound(mcx, msg, args);

      // The default is to forbid all message attributes. If the attrs prop exists
      // on the Localized instance, only set message attributes which have been
      // explicitly allowed by the developer.
      if (attrs && messageAttrs) {
        var localizedProps = {};

        for (const [name, value] of Object.entries(messageAttrs)) {
          if (attrs[name]) {
            localizedProps[name] = value;
          }
        }
      }

      // If the wrapped component is a known void element, explicitly dismiss the
      // message value and do not pass it to cloneElement in order to avoid the
      // "void element tags must neither have `children` nor use
      // `dangerouslySetInnerHTML`" error.
      if (elem.type in voidElementTags) {
        return react.cloneElement(elem, localizedProps);
      }

      // If the message has a null value, we're only interested in its attributes.
      // Do not pass the null value to cloneElement as it would nuke all children
      // of the wrapped component.
      if (messageValue === null) {
        return react.cloneElement(elem, localizedProps);
      }

      // If the message value doesn't contain any markup nor any HTML entities,
      // insert it as the only child of the wrapped component.
      if (!reMarkup.test(messageValue)) {
        return react.cloneElement(elem, localizedProps, messageValue);
      }

      // If the message contains markup, parse it and try to match the children
      // found in the translation with the props passed to this Localized.
      const translationNodes = Array.from(parseMarkup(messageValue).childNodes);
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

      return react.cloneElement(elem, localizedProps, ...translatedChildren);
    }
  }

  Localized.contextTypes = {
    l10n: isReactLocalization
  };

  Localized.propTypes = {
    children: PropTypes.element.isRequired,
  };

  /*
   * @module fluent-react
   * @overview
   *

   * `fluent-react` provides React bindings for Fluent.  It takes advantage of
   * React's Components system and the virtual DOM.  Translations are exposed to
   * components via the provider pattern.
   *
   *     <LocalizationProvider messages={…}>
   *         <Localized id="hello-world">
   *             <p>{'Hello, world!'}</p>
   *         </Localized>
   *     </LocalizationProvider>
   *
   * Consult the documentation of the `LocalizationProvider` and the `Localized`
   * components for more information.
   */

  exports.LocalizationProvider = LocalizationProvider;
  exports.withLocalization = withLocalization;
  exports.Localized = Localized;
  exports.ReactLocalization = ReactLocalization;
  exports.isReactLocalization = isReactLocalization;

  Object.defineProperty(exports, '__esModule', { value: true });

})));
