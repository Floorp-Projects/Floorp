// sweet sweet additional assertions
Object.extend(Test.Unit.Testcase.prototype, {
  assertEnabled: function() {
    for (var i = 0, element; element = arguments[i]; i++) {
      this.assert(!$(element).disabled, Test.Unit.inspect(element) + ' was disabled');
    }
  },
  assertDisabled: function() {
    for (var i = 0, element; element = arguments[i]; i++) {
      this.assert($(element).disabled, Test.Unit.inspect(element) + ' was enabled');
    }
  }
});

new Test.Unit.Runner({
  
  // Make sure to set defaults in the test forms, as some browsers override this
  // with previously entered values on page reload
  setup: function(){
    $$('form').each(function(f){ f.reset() });
    // hidden value does not reset (for some reason)
    $('bigform')['tf_hidden'].value = '';
  },
  
  testDollarF: function(){
    this.assertEqual("4", $F("input_enabled"));
  },
  
  testFormElementEventObserver: function(){
    var callbackCounter = 0;
    var observer = new Form.Element.EventObserver('input_enabled', function(){
      callbackCounter++;
    });
    
    this.assertEqual(0, callbackCounter);
    $('input_enabled').value = 'boo!';
    observer.onElementEvent(); // can't test the event directly, simulating
    this.assertEqual(1, callbackCounter);
  },

  testFormElementObserver: function(){
    var timedCounter = 0;
    // First part: regular field
    var observer = new Form.Element.Observer('input_enabled', 0.5, function() {
      ++timedCounter;
    });

    // Test it's unchanged yet
    this.assertEqual(0, timedCounter);
    // Test it doesn't change on first check
    this.wait(550, function() {
      this.assertEqual(0, timedCounter);
      // Change, test it doesn't immediately change
      $('input_enabled').value = 'yowza!';
      this.assertEqual(0, timedCounter);
      // Test it changes on next check, but not again on the next
      this.wait(550, function() {
        this.assertEqual(1, timedCounter);
        this.wait(550, function() {
          this.assertEqual(1, timedCounter);
          observer.stop();
        });
      });
    });

    // Second part: multiple-select
    [1, 2, 3].each(function(index) {
      $('multiSel1_opt' + index).selected = (1 == index);
    });
    timedCounter = 0;
    observer = new Form.Element.Observer('multiSel1', 0.5, function() {
      ++timedCounter;
    });

    // Test it's unchanged yet
    this.assertEqual(0, timedCounter);
    // Test it doesn't change on first check
    this.wait(550, function() {
      this.assertEqual(0, timedCounter);
      // Change, test it doesn't immediately change
      // NOTE: it is important that the 3rd be re-selected, for the
      // serialize form to obtain the expected value :-)
      $('multiSel1_opt3').selected = true;
      this.assertEqual(0, timedCounter);
      // Test it changes on next check, but not again on the next
      this.wait(550, function() {
        this.assertEqual(1, timedCounter);
        this.wait(550, function() {
          this.assertEqual(1, timedCounter);
          observer.stop();
        });
      });
    });
  },
  
  testFormObserver: function(){
    var timedCounter = 0;
    // should work the same way was Form.Element.Observer
    var observer = new Form.Observer('form', 0.5, function(form, value) {
      ++timedCounter;
    });

    // Test it's unchanged yet
    this.assertEqual(0, timedCounter);
    // Test it doesn't change on first check
    this.wait(550, function() {
      this.assertEqual(0, timedCounter);
      // Change, test it doesn't immediately change
      $('input_enabled').value = 'yowza!';
      this.assertEqual(0, timedCounter);
      // Test it changes on next check, but not again on the next
      this.wait(550, function() {
        this.assertEqual(1, timedCounter);
        this.wait(550, function() {
          this.assertEqual(1, timedCounter);
          observer.stop();
        });
      });
    });
  },
  
  testFormEnabling: function(){
    var form = $('bigform')
    var input1 = $('dummy_disabled');
    var input2 = $('focus_text');
    
    this.assertDisabled(input1);
    this.assertEnabled(input2);
    
    form.disable();
    this.assertDisabled(input1, input2);
    form.enable();
    this.assertEnabled(input1, input2);
    input1.disable();
    this.assertDisabled(input1);
    
    // non-form elements:
    var fieldset = $('selects_fieldset');
    var fields = fieldset.immediateDescendants();
    fields.each(function(select) { this.assertEnabled(select) }, this);
    
    Form.disable(fieldset)
    fields.each(function(select) { this.assertDisabled(select) }, this);
    
    Form.enable(fieldset)
    fields.each(function(select) { this.assertEnabled(select) }, this);
  },
  
  testFormElementEnabling: function(){
    var field = $('input_disabled');
    field.enable();
    this.assertEnabled(field);
    field.disable();
    this.assertDisabled(field);
    
    var field = $('input_enabled');
    this.assertEnabled(field);
    field.disable();
    this.assertDisabled(field);
    field.enable();
    this.assertEnabled(field);
  },

  // due to the lack of a DOM hasFocus() API method,
  // we're simulating things here a little bit
  testFormActivating: function(){
    // Firefox, IE, and Safari 2+
    function getSelection(element){
      try {
        if (typeof element.selectionStart == 'number') {
          return element.value.substring(element.selectionStart, element.selectionEnd);
        } else if (document.selection && document.selection.createRange) {
          return document.selection.createRange().text;
        }
      }
      catch(e){ return null }
    }
    
    // Form.focusFirstElement shouldn't focus disabled elements
    var element = Form.findFirstElement('bigform');
    this.assertEqual('submit', element.id);
    
    // Test IE doesn't select text on buttons
    Form.focusFirstElement('bigform');
    if (document.selection) this.assertEqual('', getSelection(element));
    
    // Form.Element.activate shouldn't select text on buttons
    element = $('focus_text');
    this.assertEqual('', getSelection(element));
      
    // Form.Element.activate should select text on text input elements
    element.activate();
    this.assertEqual('Hello', getSelection(element));

    // Form.Element.activate shouldn't raise an exception when the form or field is hidden
    this.assertNothingRaised(function() {
      $('form_focus_hidden').focusFirstElement();
    });
  },
  
  testFormGetElements: function() {
    var elements = Form.getElements('various'),
      names = $w('tf_selectOne tf_textarea tf_checkbox tf_selectMany tf_text tf_radio tf_hidden tf_password');
    this.assertEnumEqual(names, elements.pluck('name'))
  },
  
  testFormGetInputs: function() {
    var form = $('form');
    [form.getInputs(), Form.getInputs(form)].each(function(inputs){
      this.assertEqual(inputs.length, 5);
      this.assert(inputs instanceof Array);
      this.assert(inputs.all(function(input) { return (input.tagName == "INPUT"); }));
    }, this);
  },

  testFormFindFirstElement: function() {
    this.assertEqual($('ffe_checkbox'), $('ffe').findFirstElement());
    this.assertEqual($('ffe_ti_submit'), $('ffe_ti').findFirstElement());
    this.assertEqual($('ffe_ti2_checkbox'), $('ffe_ti2').findFirstElement());
  },
  
  testFormSerialize: function() {
    // form is initially empty
    var form = $('bigform');
    var expected = { tf_selectOne:'', tf_textarea:'', tf_text:'', tf_hidden:'', tf_password:'' };
    this.assertHashEqual(expected, Form.serialize('various', true));
      
    // set up some stuff
    form['tf_selectOne'].selectedIndex = 1;
    form['tf_textarea'].value = "boo hoo!";
    form['tf_text'].value = "123öäü";
    form['tf_hidden'].value = "moo%hoo&test";
    form['tf_password'].value = 'sekrit code';
    form['tf_checkbox'].checked = true;
    form['tf_radio'].checked = true;
    var expected = { tf_selectOne:1, tf_textarea:"boo hoo!", tf_text:"123öäü",
      tf_hidden:"moo%hoo&test", tf_password:'sekrit code', tf_checkbox:'on', tf_radio:'on' }

    // return params
    this.assertHashEqual(expected, Form.serialize('various', true));
    // return string
    this.assertEnumEqual(Object.toQueryString(expected).split('&').sort(),
                    Form.serialize('various').split('&').sort());
    this.assertEqual('string', typeof $('form').serialize({ hash:false }));

    // Checks that disabled element is not included in serialized form.
    $('input_enabled').enable();
    this.assertHashEqual({ val1:4, action:'blah', first_submit:'Commit it!' },
                    $('form').serialize(true));

    // should not eat empty values for duplicate names 
    $('checkbox_hack').checked = false;
    var data = Form.serialize('value_checks', true); 
    this.assertEnumEqual(['', 'siamese'], data['twin']); 
    this.assertEqual('0', data['checky']);
    
    $('checkbox_hack').checked = true; 
    this.assertEnumEqual($w('1 0'), Form.serialize('value_checks', true)['checky']);

    // all kinds of SELECT controls
    var params = Form.serialize('selects_fieldset', true);
    var expected = { 'nvm[]':['One', 'Three'], evu:'', 'evm[]':['', 'Three'] };
    this.assertHashEqual(expected, params);
    params = Form.serialize('selects_wrapper', true);
    this.assertHashEqual(Object.extend(expected, { vu:1, 'vm[]':[1, 3], nvu:'One' }), params);

    // explicit submit button
    this.assertHashEqual({ val1:4, action:'blah', second_submit:'Delete it!' },
                    $('form').serialize({ submit: 'second_submit' }));
    this.assertHashEqual({ val1:4, action:'blah' },
                    $('form').serialize({ submit: false }));
    this.assertHashEqual({ val1:4, action:'blah' },
                    $('form').serialize({ submit: 'inexistent' }));

    // file input should not be serialized  
    this.assertEqual('', $('form_with_file_input').serialize());   
  },
  
  testFormMethodsOnExtendedElements: function() {
    var form = $('form');
    this.assertEqual(Form.serialize('form'), form.serialize());
    this.assertEqual(Form.Element.serialize('input_enabled'), $('input_enabled').serialize());
    this.assertNotEqual(form.serialize, $('input_enabled').serialize);
    
    Element.addMethods('INPUT',  { anInputMethod: function(input)  { return 'input'  } });
    Element.addMethods('SELECT', { aSelectMethod: function(select) { return 'select' } });

    form = $('bigform');
    var input = form['tf_text'], select = form['tf_selectOne'];
    input._extendedByPrototype = select._extendedByPrototype = false;

    this.assert($(input).anInputMethod);
    this.assert(!input.aSelectMethod);
    this.assertEqual('input', input.anInputMethod());

    this.assert($(select).aSelectMethod);
    this.assert(!select.anInputMethod);      
    this.assertEqual('select', select.aSelectMethod());
  },
  
  testFormRequest: function() {
    request = $("form").request();
    this.assert($("form").hasAttribute("method"));
    this.assert(request.url.include("fixtures/empty.js?val1=4"));
    this.assertEqual("get", request.method);
    
    request = $("form").request({ method: "put", parameters: {val2: "hello"} });
    var url = request.url;
    
    this.assert(url.startsWith("fixtures/empty.js"));
    this.assert(url.endsWith("_method=put") || url.endsWith("_method=put&_="));
    this.assertEqual(4, request.options.parameters['val1']);
    this.assertEqual('hello', request.options.parameters['val2']);
    this.assertEqual("post", request.method);
    this.assertEqual("put", request.parameters['_method']);

    // with empty action attribute
    request = $("ffe").request({ method: 'post' });
    this.assert(request.url.include("unit/tmp/form_test.html"),
      'wrong default action for form element with empty action attribute');
  },
  
  testFormElementMethodsChaining: function(){
    var methods = $w('clear activate disable enable'),
      formElements = $('form').getElements();
    methods.each(function(method){
      formElements.each(function(element){
        var returned = element[method]();
        this.assertIdentical(element, returned);
      }, this);
    }, this);
  },

  testGetValue: function() {
    this.assertEqual('Click Me', Form.Element.getValue('button_type_button')); // Button element
    this.assertEqual(null, Form.Element.getValue('someBorkedId')); // Unknown
 	  this.assertEqual(null, Form.Element.getValue('form')); // Unsupported
 	  this.assertEqual(null, Form.Element.getValue('form')); // Unsupported
  },
  
  testSetValue: function(){
    // unkown element
    this.assertEqual(null, Form.Element.setValue('someBorkedId', 'blah')); // Unknown
    
    // test button element
    var button = $('button_type_button');
    button.setValue('I Changed');
    this.assertEqual('I Changed', button.getValue());

    // text input
    var input = $('input_enabled'), oldValue = input.getValue();
    this.assertEqual(input, input.setValue('foo'), 'setValue chaining is broken');
    this.assertEqual('foo', input.getValue(), 'value improperly set');
    input.setValue(oldValue);
    this.assertEqual(oldValue, input.getValue(), 'value improperly restored to original');

    // checkbox
    input = $('checkbox_hack');
    input.setValue(false);
    this.assertEqual(null, input.getValue(), 'checkbox should be unchecked');
    input.setValue(true);
    this.assertEqual("1", input.getValue(), 'checkbox should be checked');
    
    // selectbox
    input = $('bigform')['vu'];
    input.setValue('3');
    this.assertEqual('3', input.getValue(), 'single select option improperly set');
    input.setValue('1');
    this.assertEqual('1', input.getValue());
    
    // multiple select
    input = $('bigform')['vm[]'];
    input.setValue(['2', '3']);
    this.assertEnumEqual(['2', '3'], input.getValue(),
      'multiple select options improperly set');
    input.setValue(['1', '3']);
    this.assertEnumEqual(['1', '3'], input.getValue());
    
    // test chainability
    var input = $('input_enabled');
    this.assert(Object.isElement(button.setValue('New Value')));
    this.assert(Object.isElement(input.setValue('New Value')));
  }
});