promises.push(test_with_new_window((testWindow) => {
  // Test calling the HTMLElement constructor.
  (() => {
    SimpleTest.doesThrow(() => {
      testWindow.HTMLElement();
    }, 'calling the HTMLElement constructor should throw a TypeError');
  })();

  // Test constructing a HTMLELement.
  (() => {
    SimpleTest.doesThrow(() => {
      new testWindow.HTMLElement();
    }, 'constructing a HTMLElement should throw a TypeError');
  })();

  // Test constructing a custom element with defining HTMLElement as entry.
  (() => {
    testWindow.customElements.define('x-defining-html-element',
                                     testWindow.HTMLElement);
    SimpleTest.doesThrow(() => {
      new testWindow.HTMLElement();
    }, 'constructing a custom element with defining HTMLElement as registry ' +
       'entry should throw a TypeError');
  })();

  // Test calling a custom element constructor and constructing an autonomous
  // custom element.
  (() => {
    let num_constructor_invocations = 0;
    class X extends testWindow.HTMLElement {
      constructor() {
        super();
        num_constructor_invocations++;
      }
    }
    testWindow.customElements.define('x-element', X);
    SimpleTest.doesThrow(() => {
      X();
    }, 'calling an autonomous custom element constructor should throw a TypeError');

    let element = new X();
    SimpleTest.is(Object.getPrototypeOf(Cu.waiveXrays(element)), X.prototype,
                  'constructing an autonomous custom element; ' +
                  'the element should be a registered constructor');
    SimpleTest.is(element.localName, 'x-element',
                  'constructing an autonomous custom element; ' +
                  'the element tag name should be "x-element"');
    SimpleTest.is(element.namespaceURI, 'http://www.w3.org/1999/xhtml',
                  'constructing an autonomous custom element; ' +
                  'the element should be in the HTML namespace');
    SimpleTest.is(element.prefix, null,
                  'constructing an autonomous custom element; ' +
                  'the element name should not have a prefix');
    SimpleTest.is(element.ownerDocument, testWindow.document,
                  'constructing an autonomous custom element; ' +
                  'the element should be owned by the registry\'s associated ' +
                  'document');
    SimpleTest.is(num_constructor_invocations, 1,
                  'constructing an autonomous custom element; ' +
                  'the constructor should have been invoked once');
  })();

  // Test if prototype is no an object.
  (() => {
    function ElementWithNonObjectPrototype() {
      let o = Reflect.construct(testWindow.HTMLElement, [], new.target);
      SimpleTest.is(Object.getPrototypeOf(Cu.waiveXrays(o)), window.HTMLElement.prototype,
                    'constructing an autonomous custom element; ' +
                    'if prototype is not object, fallback from NewTarget\'s realm');
    }

     // Prototype have to be an object during define(), otherwise define will
     // throw an TypeError exception.
    ElementWithNonObjectPrototype.prototype = {};
    testWindow.customElements.define('x-non-object-prototype',
                                     ElementWithNonObjectPrototype);

    ElementWithNonObjectPrototype.prototype = "string";
    new ElementWithNonObjectPrototype();
  })();
}));
