[
  // [TagName, InterfaceName]
  ['a', 'Anchor'],
  ['abbr', ''],
  ['acronym', ''],
  ['address', ''],
  ['area', 'Area'],
  ['article', ''],
  ['aside', ''],
  ['audio', 'Audio'],
  ['b', ''],
  ['base', 'Base'],
  ['basefont', ''],
  ['bdo', ''],
  ['big', ''],
  ['blockquote', 'Quote'],
  ['body', 'Body'],
  ['br', 'BR'],
  ['button', 'Button'],
  ['canvas', 'Canvas'],
  ['caption', 'TableCaption'],
  ['center', ''],
  ['cite', ''],
  ['code', ''],
  ['col', 'TableCol'],
  ['colgroup', 'TableCol'],
  ['data', 'Data'],
  ['datalist', 'DataList'],
  ['dd', ''],
  ['del', 'Mod'],
  ['details', 'Details'],
  ['dfn', ''],
  ['dir', 'Directory'],
  ['div', 'Div'],
  ['dl', 'DList'],
  ['dt', ''],
  ['em', ''],
  ['embed', 'Embed'],
  ['fieldset', 'FieldSet'],
  ['figcaption', ''],
  ['figure', ''],
  ['font', 'Font'],
  ['footer', ''],
  ['form', 'Form'],
  ['frame', 'Frame'],
  ['frameset', 'FrameSet'],
  ['h1', 'Heading'],
  ['h2', 'Heading'],
  ['h3', 'Heading'],
  ['h4', 'Heading'],
  ['h5', 'Heading'],
  ['h6', 'Heading'],
  ['head', 'Head'],
  ['header', ''],
  ['hgroup', ''],
  ['hr', 'HR'],
  ['html', 'Html'],
  ['i', ''],
  ['iframe', 'IFrame'],
  ['image', ''],
  ['img', 'Image'],
  ['input', 'Input'],
  ['ins', 'Mod'],
  ['kbd', ''],
  ['label', 'Label'],
  ['legend', 'Legend'],
  ['li', 'LI'],
  ['link', 'Link'],
  ['listing', 'Pre'],
  ['main', ''],
  ['map', 'Map'],
  ['mark', ''],
  ['marquee', 'Div'],
  ['menu', 'Menu'],
  ['menuitem', 'MenuItem'],
  ['meta', 'Meta'],
  ['meter', 'Meter'],
  ['nav', ''],
  ['nobr', ''],
  ['noembed', ''],
  ['noframes', ''],
  ['noscript', ''],
  ['object', 'Object'],
  ['ol', 'OList'],
  ['optgroup', 'OptGroup'],
  ['option', 'Option'],
  ['output', 'Output'],
  ['p', 'Paragraph'],
  ['param', 'Param'],
  ['picture', 'Picture'],
  ['plaintext', ''],
  ['pre', 'Pre'],
  ['progress', 'Progress'],
  ['q', 'Quote'],
  ['rb', ''],
  ['rp', ''],
  ['rt', ''],
  ['rtc', ''],
  ['ruby', ''],
  ['s', ''],
  ['samp', ''],
  ['script', 'Script'],
  ['section', ''],
  ['select', 'Select'],
  ['small', ''],
  ['source', 'Source'],
  ['span', 'Span'],
  ['strike', ''],
  ['strong', ''],
  ['style', 'Style'],
  ['sub', ''],
  ['summary', ''],
  ['sup', ''],
  ['table', 'Table'],
  ['tbody', 'TableSection'],
  ['td', 'TableCell'],
  ['textarea', 'TextArea'],
  ['tfoot', 'TableSection'],
  ['th', 'TableCell'],
  ['thead', 'TableSection'],
  ['template', 'Template'],
  ['time', 'Time'],
  ['title', 'Title'],
  ['tr', 'TableRow'],
  ['track', 'Track'],
  ['tt', ''],
  ['u', ''],
  ['ul', 'UList'],
  ['var', ''],
  ['video', 'Video'],
  ['wbr', ''],
  ['xmp', 'Pre'],
].forEach((e) => {
  let tagName = e[0];
  let interfaceName = 'HTML' + e[1] + 'Element';
  promises.push(test_with_new_window((testWindow) => {
    // Use window from iframe to isolate the test.
    // Test calling the HTML*Element constructor.
    (() => {
      SimpleTest.doesThrow(() => {
        testWindow[interfaceName]();
      }, 'calling the ' + interfaceName + ' constructor should throw a TypeError');
    })();

    // Test constructing a HTML*ELement.
    (() => {
      SimpleTest.doesThrow(() => {
        new testWindow[interfaceName]();
      }, 'constructing a ' + interfaceName + ' should throw a TypeError');
    })();

    // Test constructing a custom element with defining HTML*Element as entry.
    (() => {
      testWindow.customElements.define('x-defining-' + tagName,
                                       testWindow[interfaceName]);
      SimpleTest.doesThrow(() => {
        new testWindow[interfaceName]();
      }, 'constructing a custom element with defining ' + interfaceName +
         ' as registry entry should throw a TypeError');
    })();

    // Since HTMLElement can be registered without specifying "extends", skip
    // testing HTMLElement tags.
    if (interfaceName !== "HTMLElement") {
      // Test constructing a customized HTML*Element with defining a registry entry
      // without specifying "extends".
      (() => {
        class X extends testWindow[interfaceName] {}
        testWindow.customElements.define('x-defining-invalid-' + tagName, X);
        SimpleTest.doesThrow(() => {
          new X();
        }, 'constructing a customized ' + interfaceName + ' with defining a ' +
           'registry entry without specifying "extends" should throw a TypeError');
      })();
    }

    // Test constructing a built-in custom element with defining a registry entry
    // with incorrect "extends" information.
    (() => {
      class X extends testWindow[interfaceName] {}
      testWindow.customElements.define('x-defining-incorrect-' + tagName, X,
                                       { extends: tagName === 'img' ? 'p' : 'img' });
      SimpleTest.doesThrow(() => {
        new X();
      }, 'constructing a customized ' + interfaceName + ' with defining a ' +
         'registry entry with incorrect "extends" should throw a TypeError');
    })();

    // Test calling a custom element constructor and constructing a built-in
    // custom element.
    (() => {
      let num_constructor_invocations = 0;
      class X extends testWindow[interfaceName] {
        constructor() {
          super();
          num_constructor_invocations++;
        }
      }
      testWindow.customElements.define('x-' + tagName, X, { extends: tagName });
      SimpleTest.doesThrow(() => {
        X();
      }, 'calling a customized ' + interfaceName + ' constructor should throw a TypeError');

      let element = new X();

      SimpleTest.is(Object.getPrototypeOf(Cu.waiveXrays(element)), X.prototype,
                    'constructing a customized ' + interfaceName +
                    '; the element should be a registered constructor');
      SimpleTest.is(element.localName, tagName,
                    'constructing a customized ' + interfaceName +
                    '; the element tag name should be "' + tagName + '"');
      SimpleTest.is(element.namespaceURI, 'http://www.w3.org/1999/xhtml',
                    'constructing a customized ' + interfaceName +
                    '; the element should be in the HTML namespace');
      SimpleTest.is(element.prefix, null,
                    'constructing a customized ' + interfaceName +
                    '; the element name should not have a prefix');
      SimpleTest.is(element.ownerDocument, testWindow.document,
                    'constructing a customized ' + interfaceName +
                    '; the element should be owned by the registry\'s associated ' +
                    'document');
      SimpleTest.is(num_constructor_invocations, 1,
                    'constructing a customized ' + interfaceName +
                    '; the constructor should have been invoked once');
    })();

    // Test if prototype is no an object.
    (() => {
      function ElementWithNonObjectPrototype() {
        let o = Reflect.construct(testWindow[interfaceName], [], new.target);
        SimpleTest.is(Object.getPrototypeOf(Cu.waiveXrays(o)), window[interfaceName].prototype,
                      'constructing a customized ' + interfaceName +
                      '; if prototype is not object, fallback from NewTarget\'s realm');
      }

      // Prototype have to be an object during define(), otherwise define will
      // throw an TypeError exception.
      ElementWithNonObjectPrototype.prototype = {};
      testWindow.customElements.define('x-non-object-prototype-' + tagName,
                                       ElementWithNonObjectPrototype,
                                       { extends: tagName });

      ElementWithNonObjectPrototype.prototype = "string";
      new ElementWithNonObjectPrototype();
    })();
  }));
});
