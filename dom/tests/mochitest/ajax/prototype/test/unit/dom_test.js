var getInnerHTML = function(id) {
  return $(id).innerHTML.toString().toLowerCase().gsub(/[\r\n\t]/, '');
};
var createParagraph = function(text) {
  var p = document.createElement('p');
  p.appendChild(document.createTextNode(text));
  return p;
}

new Test.Unit.Runner({
  setup: function() {
    if (documentViewportProperties) return;
    // Based on properties check from http://www.quirksmode.org/viewport/compatibility.html
    documentViewportProperties = {
      properties : [
        'self.pageXOffset', 'self.pageYOffset',
        'self.screenX', 'self.screenY',
        'self.innerHeight', 'self.innerWidth',
        'self.outerHeight', 'self.outerWidth',
        'self.screen.height', 'self.screen.width',
        'self.screen.availHeight', 'self.screen.availWidth',
        'self.screen.availTop', 'self.screen.availLeft',
        'self.screen.Top', 'self.screen.Left', 
        'self.screenTop', 'self.screenLeft',
        'document.body.clientHeight', 'document.body.clientWidth',
        'document.body.scrollHeight', 'document.body.scrollWidth',
        'document.body.scrollLeft', 'document.body.scrollTop',
        'document.body.offsetHeight', 'document.body.offsetWidth',
        'document.body.offsetTop', 'document.body.offsetLeft'
      ].inject([], function(properties, prop) {
        if (!self.screen && prop.include('self.screen')) return;
        if (!document.body && prop.include('document.body')) return;
        properties.push(prop);
        if (prop.include('.body') && document.documentElement)
          properties.push(prop.sub('.body', '.documentElement'));
        return properties;
      }),

      inspect : function() {
        var props = [];
        this.properties.each(function(prop) {
          if (eval(prop)) props[prop] = eval(prop);
        }, this);
        return props;
      }
    };
  },

  testDollarFunction: function() {
    this.assertUndefined($());
    
    this.assertNull(document.getElementById('noWayThisIDExists'));
    this.assertNull($('noWayThisIDExists'));
    
    this.assertIdentical(document.getElementById('testdiv'), $('testdiv'));
    this.assertEnumEqual([ $('testdiv'), $('container') ], $('testdiv', 'container'));
    this.assertEnumEqual([ $('testdiv'), undefined, $('container') ],
      $('testdiv', 'noWayThisIDExists', 'container'));
    var elt = $('testdiv');
    this.assertIdentical(elt, $(elt));
    this.assertRespondsTo('hide', elt);
    this.assertRespondsTo('childOf', elt);
  },
  
  testGetElementsByClassName: function() {
    if (document.getElementsByClassName.toString().include('[native code]')) {
      this.info("browser uses native getElementsByClassName; skipping tests");
      return;
    } 

    
    var div = $('class_names'), list = $('class_names_ul');
    
    this.assertElementsMatch(document.getElementsByClassName('A'), 'p.A', 'ul#class_names_ul.A', 'li.A.C');
    
    if (Prototype.Browser.IE)
      this.assertUndefined(document.getElementById('unextended').show);
    
    this.assertElementsMatch(div.getElementsByClassName('B'), 'ul#class_names_ul.A.B', 'div.B.C.D');
    this.assertElementsMatch(div.getElementsByClassName('D C B'), 'div.B.C.D');
    this.assertElementsMatch(div.getElementsByClassName(' D\nC\tB '), 'div.B.C.D');
    this.assertElementsMatch(div.getElementsByClassName($w('D C B')));
    this.assertElementsMatch(list.getElementsByClassName('A'), 'li.A.C');
    this.assertElementsMatch(list.getElementsByClassName(' A '), 'li.A.C');
    this.assertElementsMatch(list.getElementsByClassName('C A'), 'li.A.C');
    this.assertElementsMatch(list.getElementsByClassName("C\nA "), 'li.A.C');
    this.assertElementsMatch(list.getElementsByClassName('B'));
    this.assertElementsMatch(list.getElementsByClassName('1'), 'li.1');
    this.assertElementsMatch(list.getElementsByClassName([1]), 'li.1');
    this.assertElementsMatch(list.getElementsByClassName(['1 junk']));
    this.assertElementsMatch(list.getElementsByClassName(''));
    this.assertElementsMatch(list.getElementsByClassName(' '));
    this.assertElementsMatch(list.getElementsByClassName(['']));
    this.assertElementsMatch(list.getElementsByClassName([' ', '']));
    this.assertElementsMatch(list.getElementsByClassName({}));
    
    // those lookups shouldn't have extended all nodes in document
    if (Prototype.Browser.IE) this.assertUndefined(document.getElementById('unextended')['show']);
  },

  testElementInsertWithHTML: function() {
    Element.insert('insertions-main', {before:'<p><em>before</em> text</p><p>more testing</p>'});
    this.assert(getInnerHTML('insertions-container').startsWith('<p><em>before</em> text</p><p>more testing</p>'));
    Element.insert('insertions-main', {after:'<p><em>after</em> text</p><p>more testing</p>'});
    this.assert(getInnerHTML('insertions-container').endsWith('<p><em>after</em> text</p><p>more testing</p>'));
    Element.insert('insertions-main', {top:'<p><em>top</em> text.</p><p>more testing</p>'});
    this.assert(getInnerHTML('insertions-main').startsWith('<p><em>top</em> text.</p><p>more testing</p>'));
    Element.insert('insertions-main', {bottom:'<p><em>bottom</em> text.</p><p>more testing</p>'});
    this.assert(getInnerHTML('insertions-main').endsWith('<p><em>bottom</em> text.</p><p>more testing</p>'));
  },

  testElementInsertWithDOMNode: function() {
    Element.insert('insertions-node-main', {before: createParagraph('node before')});
    this.assert(getInnerHTML('insertions-node-container').startsWith('<p>node before</p>'));
    Element.insert('insertions-node-main', {after: createParagraph('node after')});
    this.assert(getInnerHTML('insertions-node-container').endsWith('<p>node after</p>'));
    Element.insert('insertions-node-main', {top:createParagraph('node top')});
    this.assert(getInnerHTML('insertions-node-main').startsWith('<p>node top</p>'));
    Element.insert('insertions-node-main', {bottom:createParagraph('node bottom')});
    this.assert(getInnerHTML('insertions-node-main').endsWith('<p>node bottom</p>'));
    this.assertEqual($('insertions-node-main'), $('insertions-node-main').insert(document.createElement('p')));
  },
  
  testElementInsertWithToElementMethod: function() {
    Element.insert('insertions-node-main', {toElement: createParagraph.curry('toElement') });
    this.assert(getInnerHTML('insertions-node-main').endsWith('<p>toelement</p>'));
    Element.insert('insertions-node-main', {bottom: {toElement: createParagraph.curry('bottom toElement') }});
    this.assert(getInnerHTML('insertions-node-main').endsWith('<p>bottom toelement</p>'));
  },
  
  testElementInsertWithToHTMLMethod: function() {
    Element.insert('insertions-node-main', {toHTML: function() { return '<p>toHTML</p>'} });
    this.assert(getInnerHTML('insertions-node-main').endsWith('<p>tohtml</p>'));
    Element.insert('insertions-node-main', {bottom: {toHTML: function() { return '<p>bottom toHTML</p>'} }});
    this.assert(getInnerHTML('insertions-node-main').endsWith('<p>bottom tohtml</p>'));
  },
  
  testElementInsertWithNonString: function() {
    Element.insert('insertions-main', {bottom:3});
    this.assert(getInnerHTML('insertions-main').endsWith('3'));
  },

  testElementInsertInTables: function() {
    Element.insert('second_row', {after:'<tr id="third_row"><td>Third Row</td></tr>'});
    this.assert($('second_row').descendantOf('table'));
    
    $('a_cell').insert({top:'hello world'});
    this.assert($('a_cell').innerHTML.startsWith('hello world'));
    $('a_cell').insert({after:'<td>hi planet</td>'});
    this.assertEqual('hi planet', $('a_cell').next().innerHTML);
    $('table_for_insertions').insert('<tr><td>a cell!</td></tr>');
    this.assert($('table_for_insertions').innerHTML.gsub('\r\n', '').toLowerCase().include('<tr><td>a cell!</td></tr>'));
    $('row_1').insert({after:'<tr></tr><tr></tr><tr><td>last</td></tr>'});
    this.assertEqual('last', $A($('table_for_row_insertions').getElementsByTagName('tr')).last().lastChild.innerHTML);
  },
  
  testElementInsertInSelect: function() {
    var selectTop = $('select_for_insert_top'), selectBottom = $('select_for_insert_bottom');
    selectBottom.insert('<option value="33">option 33</option><option selected="selected">option 45</option>');
    this.assertEqual('option 45', selectBottom.getValue());
    selectTop.insert({top:'<option value="A">option A</option><option value="B" selected="selected">option B</option>'});
    this.assertEqual(4, selectTop.options.length);
  },
      
  testElementMethodInsert: function() {
    $('element-insertions-main').insert({before:'some text before'});
    this.assert(getInnerHTML('element-insertions-container').startsWith('some text before'));
    $('element-insertions-main').insert({after:'some text after'});
    this.assert(getInnerHTML('element-insertions-container').endsWith('some text after'));
    $('element-insertions-main').insert({top:'some text top'});
    this.assert(getInnerHTML('element-insertions-main').startsWith('some text top'));
    $('element-insertions-main').insert({bottom:'some text bottom'});
    this.assert(getInnerHTML('element-insertions-main').endsWith('some text bottom'));
    
    $('element-insertions-main').insert('some more text at the bottom');
    this.assert(getInnerHTML('element-insertions-main').endsWith('some more text at the bottom'));
    
    $('element-insertions-main').insert({TOP:'some text uppercase top'});
    this.assert(getInnerHTML('element-insertions-main').startsWith('some text uppercase top'));
    
    $('element-insertions-multiple-main').insert({
      top:'1', bottom:2, before: new Element('p').update('3'), after:'4'
    });
    this.assert(getInnerHTML('element-insertions-multiple-main').startsWith('1'));
    this.assert(getInnerHTML('element-insertions-multiple-main').endsWith('2'));
    this.assert(getInnerHTML('element-insertions-multiple-container').startsWith('<p>3</p>'));
    this.assert(getInnerHTML('element-insertions-multiple-container').endsWith('4'));
    
    $('element-insertions-main').update('test');
    $('element-insertions-main').insert(null);
    $('element-insertions-main').insert({bottom:null});
    this.assertEqual('test', getInnerHTML('element-insertions-main'));
    $('element-insertions-main').insert(1337);
    this.assertEqual('test1337', getInnerHTML('element-insertions-main'));
  },
  
  testNewElementInsert: function() {
    var container = new Element('div');
    element = new Element('div');
    container.insert(element);
    
    element.insert({ before: '<p>a paragraph</p>' });
    this.assertEqual('<p>a paragraph</p><div></div>', getInnerHTML(container));
    element.insert({ after: 'some text' });
    this.assertEqual('<p>a paragraph</p><div></div>some text', getInnerHTML(container));
    
    element.insert({ top: '<p>a paragraph</p>' });
    this.assertEqual('<p>a paragraph</p>', getInnerHTML(element));
    element.insert('some text');
    this.assertEqual('<p>a paragraph</p>some text', getInnerHTML(element));
  },
  
  testInsertionBackwardsCompatibility: function() {
    new Insertion.Before('element-insertions-main', 'some backward-compatibility testing before');
    this.assert(getInnerHTML('element-insertions-container').include('some backward-compatibility testing before'));
    new Insertion.After('element-insertions-main', 'some backward-compatibility testing after');
    this.assert(getInnerHTML('element-insertions-container').include('some backward-compatibility testing after'));
    new Insertion.Top('element-insertions-main', 'some backward-compatibility testing top');
    this.assert(getInnerHTML('element-insertions-main').startsWith('some backward-compatibility testing top'));
    new Insertion.Bottom('element-insertions-main', 'some backward-compatibility testing bottom');
    this.assert(getInnerHTML('element-insertions-main').endsWith('some backward-compatibility testing bottom'));
  },
  
  testElementWrap: function() {
    var element = $('wrap'), parent = document.createElement('div');
    element.wrap();
    this.assert(getInnerHTML('wrap-container').startsWith('<div><p'));
    element.wrap('div');
    this.assert(getInnerHTML('wrap-container').startsWith('<div><div><p'));
    
    element.wrap(parent);
    this.assert(Object.isFunction(parent.setStyle));
    this.assert(getInnerHTML('wrap-container').startsWith('<div><div><div><p'));
    
    element.wrap('div', {className: 'wrapper'});
    this.assert(element.up().hasClassName('wrapper'));      
    element.wrap({className: 'other-wrapper'});
    this.assert(element.up().hasClassName('other-wrapper'));
    element.wrap(new Element('div'), {className: 'yet-other-wrapper'});
    this.assert(element.up().hasClassName('yet-other-wrapper'));
    
    var orphan = new Element('p'), div = new Element('div');
    orphan.wrap(div);
    this.assertEqual(orphan.parentNode, div);
  },
  
  testElementWrapReturnsWrapper: function() {
    var element = new Element("div");
    var wrapper = element.wrap("div");
    this.assertNotEqual(element, wrapper);
    this.assertEqual(element.up(), wrapper);
  },
  
  testElementVisible: function(){
    this.assertNotEqual('none', $('test-visible').style.display);
    this.assertEqual('none', $('test-hidden').style.display);
  },
  
  testElementToggle: function(){
    $('test-toggle-visible').toggle();
    this.assert(!$('test-toggle-visible').visible());
    $('test-toggle-visible').toggle();
    this.assert($('test-toggle-visible').visible());
    $('test-toggle-hidden').toggle();
    this.assert($('test-toggle-hidden').visible());
    $('test-toggle-hidden').toggle();
    this.assert(!$('test-toggle-hidden').visible());
  },
  
  testElementShow: function(){
    $('test-show-visible').show();
    this.assert($('test-show-visible').visible());
    $('test-show-hidden').show();
    this.assert($('test-show-hidden').visible());
  },
  
  testElementHide: function(){
    $('test-hide-visible').hide();
    this.assert(!$('test-hide-visible').visible());
    $('test-hide-hidden').hide();
    this.assert(!$('test-hide-hidden').visible());
  }, 
  
  testElementRemove: function(){
    $('removable').remove();
    this.assert($('removable-container').empty());
  },  
   
  testElementUpdate: function() {
    $('testdiv').update('hello from div!');
    this.assertEqual('hello from div!', $('testdiv').innerHTML);
    
    Element.update('testdiv', 'another hello from div!');
    this.assertEqual('another hello from div!', $('testdiv').innerHTML);
    
    Element.update('testdiv', 123);
    this.assertEqual('123', $('testdiv').innerHTML);
    
    Element.update('testdiv');
    this.assertEqual('', $('testdiv').innerHTML);
    
    Element.update('testdiv', '&nbsp;');
    this.assert(!$('testdiv').innerHTML.empty());
  },

  testElementUpdateWithScript: function() {
    $('testdiv').update('hello from div!<script>\ntestVar="hello!";\n</'+'script>');
    this.assertEqual('hello from div!',$('testdiv').innerHTML);
    this.wait(100,function(){
      this.assertEqual('hello!',testVar);
      
      Element.update('testdiv','another hello from div!\n<script>testVar="another hello!"</'+'script>\nhere it goes');
      
      // note: IE normalizes whitespace (like line breaks) to single spaces, thus the match test
      this.assertMatch(/^another hello from div!\s+here it goes$/,$('testdiv').innerHTML);
      this.wait(100,function(){
        this.assertEqual('another hello!',testVar);
        
        Element.update('testdiv','a\n<script>testVar="a"\ntestVar="b"</'+'script>');
        this.wait(100,function(){
          this.assertEqual('b', testVar);
          
          Element.update('testdiv',
            'x<script>testVar2="a"</'+'script>\nblah\n'+
            'x<script>testVar2="b"</'+'script>');
          this.wait(100,function(){
            this.assertEqual('b', testVar2);
          });
        });
      });
    });
  },
  
  testElementUpdateInTableRow: function() {
    $('second_row').update('<td id="i_am_a_td">test</td>');
    this.assertEqual('test',$('i_am_a_td').innerHTML);

    Element.update('second_row','<td id="i_am_a_td">another <span>test</span></td>');
    this.assertEqual('another <span>test</span>',$('i_am_a_td').innerHTML.toLowerCase());
  },
  
  testElementUpdateInTableCell: function() {
    Element.update('a_cell','another <span>test</span>');
    this.assertEqual('another <span>test</span>',$('a_cell').innerHTML.toLowerCase());
  },
  
  testElementUpdateInTable: function() {
    Element.update('table','<tr><td>boo!</td></tr>');
    this.assertMatch(/^<tr>\s*<td>boo!<\/td><\/tr>$/,$('table').innerHTML.toLowerCase());
  },
  
  testElementUpdateInSelect: function() {
    var select = $('select_for_update');
    select.update('<option value="3">option 3</option><option selected="selected">option 4</option>');
    this.assertEqual('option 4', select.getValue());
  },

  testElementUpdateWithDOMNode: function() {
    $('testdiv').update(new Element('div').insert('bla'));
    this.assertEqual('<div>bla</div>', getInnerHTML('testdiv'));
  },
  
  testElementUpdateWithToElementMethod: function() {
    $('testdiv').update({toElement: createParagraph.curry('foo')});
    this.assertEqual('<p>foo</p>', getInnerHTML('testdiv'));
  },
  
  testElementUpdateWithToHTMLMethod: function() {
    $('testdiv').update({toHTML: function() { return 'hello world' }});
    this.assertEqual('hello world', getInnerHTML('testdiv'));
  },
  
  testElementReplace: function() {
    $('testdiv-replace-1').replace('hello from div!');
    this.assertEqual('hello from div!', $('testdiv-replace-container-1').innerHTML);
    
    $('testdiv-replace-2').replace(123);
    this.assertEqual('123', $('testdiv-replace-container-2').innerHTML);
    
    $('testdiv-replace-3').replace();
    this.assertEqual('', $('testdiv-replace-container-3').innerHTML);
    
    $('testrow-replace').replace('<tr><td>hello</td></tr>');
    this.assert(getInnerHTML('testrow-replace-container').include('<tr><td>hello</td></tr>'));
    
    $('testoption-replace').replace('<option>hello</option>');
    this.assert($('testoption-replace-container').innerHTML.include('hello'));
    
    Element.replace('testinput-replace', '<p>hello world</p>');
    this.assertEqual('<p>hello world</p>', getInnerHTML('testform-replace'));

    Element.replace('testform-replace', '<form></form>');
    this.assertEqual('<p>some text</p><form></form><p>some text</p>', getInnerHTML('testform-replace-container'));
  },
  
  testElementReplaceWithScript: function() {
    $('testdiv-replace-4').replace('hello from div!<script>testVarReplace="hello!"</'+'script>');
    this.assertEqual('hello from div!', $('testdiv-replace-container-4').innerHTML);
    this.wait(100,function(){
      this.assertEqual('hello!',testVarReplace);
      
      $('testdiv-replace-5').replace('another hello from div!\n<script>testVarReplace="another hello!"</'+'script>\nhere it goes');
      
      // note: IE normalizes whitespace (like line breaks) to single spaces, thus the match test
      this.assertMatch(/^another hello from div!\s+here it goes$/,$('testdiv-replace-container-5').innerHTML);
      this.wait(100,function(){
        this.assertEqual('another hello!',testVarReplace);
      });
    });
  },

  testElementReplaceWithDOMNode: function() {
    $('testdiv-replace-element').replace(createParagraph('hello'));
    this.assertEqual('<p>hello</p>', getInnerHTML('testdiv-replace-container-element'));
  },
  
  testElementReplaceWithToElementMethod: function() {
    $('testdiv-replace-toelement').replace({toElement: createParagraph.curry('hello')});
    this.assertEqual('<p>hello</p>', getInnerHTML('testdiv-replace-container-toelement'));
  },
  
  testElementReplaceWithToHTMLMethod: function() {
    $('testdiv-replace-tohtml').replace({toHTML: function() { return 'hello' }});
    this.assertEqual('hello', getInnerHTML('testdiv-replace-container-tohtml'));
  },
      
  testElementSelectorMethod: function() {      
    ['getElementsBySelector','select'].each(function(method) {
      var testSelector = $('container')[method]('p.test');
      this.assertEqual(testSelector.length, 4);
      this.assertEqual(testSelector[0], $('intended'));
      this.assertEqual(testSelector[0], $$('#container p.test')[0]);        
    }, this);
  },
  
  testElementAdjacent: function() {
    var elements = $('intended').adjacent('p');
    this.assertEqual(elements.length, 3);
    elements.each(function(element){
      this.assert(element != $('intended'));
    }, this);
  },
  
  testElementIdentify: function() {
    var parent = $('identification');
    this.assertEqual(parent.down().identify(), 'predefined_id');
    this.assert(parent.down(1).identify().startsWith('anonymous_element_'));
    this.assert(parent.down(2).identify().startsWith('anonymous_element_'));
    this.assert(parent.down(3).identify().startsWith('anonymous_element_'));
    
    this.assert(parent.down(3).id !== parent.down(2).id);
  },
     
  testElementClassNameMethod: function() {
    var testClassNames = $('container').getElementsByClassName('test');
    var testSelector = $('container').getElementsBySelector('p.test');
    this.assertEqual(testClassNames[0], $('intended'));
    this.assertEqual(testClassNames.length, 4);
    this.assertEqual(testSelector[3], testClassNames[3]);
    this.assertEqual(testClassNames.length, testSelector.length);
  },
  
  testElementAncestors: function() {
    var ancestors = $('navigation_test_f').ancestors();
    this.assertElementsMatch(ancestors, 'ul', 'li', 'ul#navigation_test',
      'div#nav_tests_isolator', 'body', 'html');
    this.assertElementsMatch(ancestors.last().ancestors());
    
    var dummy = $(document.createElement('DIV'));
    dummy.innerHTML = '<div></div>'.times(3);
    this.assert(typeof $(dummy.childNodes[0]).ancestors()[0]['setStyle'] == 'function');
  },
  
  testElementDescendants: function() {
    this.assertElementsMatch($('navigation_test').descendants(), 
      'li', 'em', 'li', 'em.dim', 'li', 'em', 'ul', 'li',
      'em.dim', 'li#navigation_test_f', 'em', 'li', 'em');
    this.assertElementsMatch($('navigation_test_f').descendants(), 'em');
    
    var dummy = $(document.createElement('DIV'));
    dummy.innerHTML = '<div></div>'.times(3);
    this.assert(typeof dummy.descendants()[0].setStyle == 'function');
  },
  
  testElementFirstDescendant: function() {
    this.assertElementMatches($('navigation_test').firstDescendant(), 'li.first');
    this.assertNull($('navigation_test_next_sibling').firstDescendant());
  },
  
  testElementChildElements: function() {
    this.assertElementsMatch($('navigation_test').childElements(),
      'li.first', 'li', 'li#navigation_test_c', 'li.last');
    this.assertNotEqual(0, $('navigation_test_next_sibling').childNodes.length);
    this.assertEnumEqual([], $('navigation_test_next_sibling').childElements());
    
    var dummy = $(document.createElement('DIV'));
    dummy.innerHTML = '<div></div>'.times(3);
    this.assert(typeof dummy.childElements()[0].setStyle == 'function');
  },

  testElementImmediateDescendants: function() {
    this.assertIdentical(Element.Methods.childElements, Element.Methods.immediateDescendants);
  },  
      
  testElementPreviousSiblings: function() {
    this.assertElementsMatch($('navigation_test').previousSiblings(),
      'span#nav_test_prev_sibling', 'p.test', 'div', 'div#nav_test_first_sibling');
    this.assertElementsMatch($('navigation_test_f').previousSiblings(), 'li');
    
    var dummy = $(document.createElement('DIV'));
    dummy.innerHTML = '<div></div>'.times(3);
    this.assert(typeof $(dummy.childNodes[1]).previousSiblings()[0].setStyle == 'function');
  },
  
  testElementNextSiblings: function() {
    this.assertElementsMatch($('navigation_test').nextSiblings(),
      'div#navigation_test_next_sibling', 'p');
    this.assertElementsMatch($('navigation_test_f').nextSiblings());
    
    var dummy = $(document.createElement('DIV'));
    dummy.innerHTML = '<div></div>'.times(3);
    this.assert(typeof $(dummy.childNodes[0]).nextSiblings()[0].setStyle == 'function');
  },
  
  testElementSiblings: function() {
    this.assertElementsMatch($('navigation_test').siblings(),
      'div#nav_test_first_sibling', 'div', 'p.test',
      'span#nav_test_prev_sibling', 'div#navigation_test_next_sibling', 'p');
      
    var dummy = $(document.createElement('DIV'));
    dummy.innerHTML = '<div></div>'.times(3);
    this.assert(typeof $(dummy.childNodes[0]).siblings()[0].setStyle == 'function');
  },
  
  testElementUp: function() {
    var element = $('navigation_test_f');
    this.assertElementMatches(element.up(), 'ul');
    this.assertElementMatches(element.up(0), 'ul');
    this.assertElementMatches(element.up(1), 'li');
    this.assertElementMatches(element.up(2), 'ul#navigation_test');
    this.assertElementsMatch(element.up('li').siblings(), 'li.first', 'li', 'li.last');
    this.assertElementMatches(element.up('ul', 1), 'ul#navigation_test');
    this.assertEqual(undefined, element.up('garbage'));
    this.assertEqual(undefined, element.up(6));
    this.assertElementMatches(element.up('.non-existant, ul'), 'ul');
    
    var dummy = $(document.createElement('DIV'));
    dummy.innerHTML = '<div></div>'.times(3);
    this.assert(typeof $(dummy.childNodes[0]).up().setStyle == 'function');
  },
  
  testElementDown: function() {
    var element = $('navigation_test');
    this.assertElementMatches(element.down(), 'li.first');
    this.assertElementMatches(element.down(0), 'li.first');
    this.assertElementMatches(element.down(1), 'em');
    this.assertElementMatches(element.down('li', 5), 'li.last');
    this.assertElementMatches(element.down('ul').down('li', 1), 'li#navigation_test_f');
    this.assertElementMatches(element.down('.non-existant, .first'), 'li.first');
    
    var dummy = $(document.createElement('DIV'));
    dummy.innerHTML = '<div></div>'.times(3);
    this.assert(typeof dummy.down().setStyle == 'function');
    
    var input = $$('input')[0];
    this.assertNothingRaised(function(){ input.down('span') });
    this.assertUndefined(input.down('span'));
  },
  
  testElementPrevious: function() {
    var element = $('navigation_test').down('li.last');
    this.assertElementMatches(element.previous(), 'li#navigation_test_c');
    this.assertElementMatches(element.previous(1), 'li');
    this.assertElementMatches(element.previous('.first'), 'li.first');
    this.assertEqual(undefined, element.previous(3));
    this.assertEqual(undefined, $('navigation_test').down().previous());
    this.assertElementMatches(element.previous('.non-existant, .first'), 'li.first');
    
    var dummy = $(document.createElement('DIV'));
    dummy.innerHTML = '<div></div>'.times(3);
    this.assert(typeof $(dummy.childNodes[1]).previous().setStyle == 'function');
  },
  
  testElementNext: function() {
    var element = $('navigation_test').down('li.first');
    this.assertElementMatches(element.next(), 'li');
    this.assertElementMatches(element.next(1), 'li#navigation_test_c');
    this.assertElementMatches(element.next(2), 'li.last');
    this.assertElementMatches(element.next('.last'), 'li.last');
    this.assertEqual(undefined, element.next(3));
    this.assertEqual(undefined, element.next(2).next());
    this.assertElementMatches(element.next('.non-existant, .last'), 'li.last');
    
    var dummy = $(document.createElement('DIV'));
    dummy.innerHTML = '<div></div>'.times(3);
    this.assert(typeof $(dummy.childNodes[0]).next().setStyle == 'function');
  },
  
  testElementInspect: function() {
    this.assertEqual('<ul id="navigation_test">', $('navigation_test').inspect());
    this.assertEqual('<li class="first">', $('navigation_test').down().inspect());
    this.assertEqual('<em>', $('navigation_test').down(1).inspect());
  },
  
  testElementMakeClipping: function() {
    var chained = Element.extend(document.createElement('DIV'));
    this.assertEqual(chained, chained.makeClipping());
    this.assertEqual(chained, chained.makeClipping());
    this.assertEqual(chained, chained.makeClipping().makeClipping());
    
    this.assertEqual(chained, chained.undoClipping());
    this.assertEqual(chained, chained.undoClipping());
    this.assertEqual(chained, chained.undoClipping().makeClipping());
    
    ['hidden','visible','scroll'].each( function(overflowValue) {
      var element = $('element_with_'+overflowValue+'_overflow');
      
      this.assertEqual(overflowValue, element.getStyle('overflow'));
      element.makeClipping();
      this.assertEqual('hidden', element.getStyle('overflow'));
      element.undoClipping();
      this.assertEqual(overflowValue, element.getStyle('overflow'));
    }, this);
  },
  
  testElementExtend: function() {
    var element = $('element_extend_test');
    this.assertRespondsTo('show', element);
    
    var XHTML_TAGS = $w(
      'a abbr acronym address applet area '+
      'b bdo big blockquote br button caption '+
      'cite code col colgroup dd del dfn div dl dt '+
      'em fieldset form h1 h2 h3 h4 h5 h6 hr '+
      'i iframe img input ins kbd label legend li '+
      'map object ol optgroup option p param pre q samp '+
      'script select small span strong style sub sup '+
      'table tbody td textarea tfoot th thead tr tt ul var');
      
    XHTML_TAGS.each(function(tag) {
      var element = document.createElement(tag);
      this.assertEqual(element, Element.extend(element));
      this.assertRespondsTo('show', element);
    }, this);
    
    [null,'','a','aa'].each(function(content) {
      var textnode = document.createTextNode(content);
      this.assertEqual(textnode, Element.extend(textnode));
      this.assert(typeof textnode['show'] == 'undefined');
    }, this);
  },
  
  testElementExtendReextendsDiscardedNodes: function() {
    this.assertRespondsTo('show', $('discard_1'));
    $('element_reextend_test').innerHTML += '<div id="discard_2"></div>';
    this.assertRespondsTo('show', $('discard_1'));
  },
  
  testElementCleanWhitespace: function() {
    Element.cleanWhitespace("test_whitespace");
    this.assertEqual(3, $("test_whitespace").childNodes.length);
    
    this.assertEqual(1, $("test_whitespace").firstChild.nodeType);
    this.assertEqual('SPAN', $("test_whitespace").firstChild.tagName);
    
    this.assertEqual(1, $("test_whitespace").firstChild.nextSibling.nodeType);
    this.assertEqual('DIV', $("test_whitespace").firstChild.nextSibling.tagName);
    
    this.assertEqual(1, $("test_whitespace").firstChild.nextSibling.nextSibling.nodeType);
    this.assertEqual('SPAN', $("test_whitespace").firstChild.nextSibling.nextSibling.tagName);
    
    var element = document.createElement('DIV');
    element.appendChild(document.createTextNode(''));
    element.appendChild(document.createTextNode(''));
    this.assertEqual(2, element.childNodes.length);
    Element.cleanWhitespace(element);
    this.assertEqual(0, element.childNodes.length);
  },
  
  testElementEmpty: function() {
    this.assert($('test-empty').empty());
    this.assert($('test-empty-but-contains-whitespace').empty());
    this.assert(!$('test-full').empty());
  },

  testDescendantOf: function() {
    this.assert($('child').descendantOf('ancestor'));
    this.assert($('child').descendantOf($('ancestor')));
    
    this.assert(!$('ancestor').descendantOf($('child')));

    this.assert($('great-grand-child').descendantOf('ancestor'), 'great-grand-child < ancestor');
    this.assert($('grand-child').descendantOf('ancestor'), 'grand-child < ancestor');
    this.assert($('great-grand-child').descendantOf('grand-child'), 'great-grand-child < grand-child');
    this.assert($('grand-child').descendantOf('child'), 'grand-child < child');
    this.assert($('great-grand-child').descendantOf('child'), 'great-grand-child < child');
    
    this.assert($('sibling').descendantOf('ancestor'), 'sibling < ancestor');
    this.assert($('grand-sibling').descendantOf('sibling'), 'grand-sibling < sibling');
    this.assert($('grand-sibling').descendantOf('ancestor'), 'grand-sibling < ancestor');
    
    this.assert($('grand-sibling').descendantOf(document.body), 'grand-sibling < body');      
    
    this.assert(!$('great-grand-child').descendantOf('great-grand-child'), 'great-grand-child < great-grand-child');
    this.assert(!$('great-grand-child').descendantOf('sibling'), 'great-grand-child < sibling');
    this.assert(!$('sibling').descendantOf('child'), 'sibling < child');
    this.assert(!$('great-grand-child').descendantOf('not-in-the-family'), 'great-grand-child < not-in-the-family');
    this.assert(!$('child').descendantOf('not-in-the-family'), 'child < not-in-the-family');
    
    this.assert(!$(document.body).descendantOf('great-grand-child'));

    // dynamically-created elements
    $('ancestor').insert(new Element('div', { id: 'weird-uncle' }));
    this.assert($('weird-uncle').descendantOf('ancestor'));
    
    $(document.body).insert(new Element('div', { id: 'impostor' }));
    this.assert(!$('impostor').descendantOf('ancestor'));
    
    // test descendantOf document
    this.assert($(document.body).descendantOf(document));  
    this.assert($(document.documentElement).descendantOf(document));  
  },  
  
  testChildOf: function() {
    this.assert($('child').childOf('ancestor'));
    this.assert($('child').childOf($('ancestor')));
    this.assert($('great-grand-child').childOf('ancestor'));
    this.assert(!$('great-grand-child').childOf('not-in-the-family'));
    this.assertIdentical(Element.Methods.childOf, Element.Methods.descendantOf);
  },    
  
  testElementSetStyle: function() {
    Element.setStyle('style_test_3',{ 'left': '2px' });
    this.assertEqual('2px', $('style_test_3').style.left);
    
    Element.setStyle('style_test_3',{ marginTop: '1px' });
    this.assertEqual('1px', $('style_test_3').style.marginTop);
    
    $('style_test_3').setStyle({ marginTop: '2px', left: '-1px' });
    this.assertEqual('-1px', $('style_test_3').style.left);
    this.assertEqual('2px', $('style_test_3').style.marginTop);
    
    this.assertEqual('none', $('style_test_3').getStyle('float'));
    $('style_test_3').setStyle({ 'float': 'left' });
    this.assertEqual('left', $('style_test_3').getStyle('float'));
    
    $('style_test_3').setStyle({ cssFloat: 'none' });
    this.assertEqual('none', $('style_test_3').getStyle('float'));
    
    this.assertEqual(1, $('style_test_3').getStyle('opacity'));
    
    $('style_test_3').setStyle({ opacity: 0.5 });
    this.assertEqual(0.5, $('style_test_3').getStyle('opacity'));
    
    $('style_test_3').setStyle({ opacity: '' });
    this.assertEqual(1, $('style_test_3').getStyle('opacity'));
    
    $('style_test_3').setStyle({ opacity: 0 });
    this.assertEqual(0, $('style_test_3').getStyle('opacity'));

    $('test_csstext_1').setStyle('font-size: 15px');
    this.assertEqual('15px', $('test_csstext_1').getStyle('font-size'));
    
    $('test_csstext_2').setStyle({height: '40px'});
    $('test_csstext_2').setStyle('font-size: 15px');
    this.assertEqual('15px', $('test_csstext_2').getStyle('font-size'));
    this.assertEqual('40px', $('test_csstext_2').getStyle('height'));
    
    $('test_csstext_3').setStyle('font-size: 15px');
    this.assertEqual('15px', $('test_csstext_3').getStyle('font-size'));
    this.assertEqual('1px', $('test_csstext_3').getStyle('border-top-width'));
    
    $('test_csstext_4').setStyle('font-size: 15px');
    this.assertEqual('15px', $('test_csstext_4').getStyle('font-size'));
    
    $('test_csstext_4').setStyle('float: right; font-size: 10px');
    this.assertEqual('right', $('test_csstext_4').getStyle('float'));
    this.assertEqual('10px', $('test_csstext_4').getStyle('font-size'));
    
    $('test_csstext_5').setStyle('float: left; opacity: .5; font-size: 10px');
    this.assertEqual(parseFloat('0.5'), parseFloat($('test_csstext_5').getStyle('opacity')));
 },
  
  testElementSetStyleCamelized: function() {
    this.assertNotEqual('30px', $('style_test_3').style.marginTop);
    $('style_test_3').setStyle({ marginTop: '30px'}, true);
    this.assertEqual('30px', $('style_test_3').style.marginTop);
  },
  
  testElementSetOpacity: function() {
    [0,0.1,0.5,0.999].each(function(opacity){
      $('style_test_3').setOpacity(opacity);
      
      // b/c of rounding issues on IE special case
      var realOpacity = $('style_test_3').getStyle('opacity');
      
      // opera rounds off to two significant digits, so we check for a
      // ballpark figure
      this.assert((Number(realOpacity) - opacity) <= 0.002, 'setting opacity to ' + opacity);        
    }, this);
    
    this.assertEqual(0,
      $('style_test_3').setOpacity(0.0000001).getStyle('opacity'));
    
    // for Firefox, we don't set to 1, because of flickering
    this.assert(
      $('style_test_3').setOpacity(0.9999999).getStyle('opacity') > 0.999
    );
    if (Prototype.Browser.IE) {
      this.assert($('style_test_4').setOpacity(0.5).currentStyle.hasLayout);
      this.assert(2, $('style_test_5').setOpacity(0.5).getStyle('zoom'));
      this.assert(0.5, new Element('div').setOpacity(0.5).getOpacity());
      this.assert(2, new Element('div').setOpacity(0.5).setStyle('zoom: 2;').getStyle('zoom'));
      this.assert(2, new Element('div').setStyle('zoom: 2;').setOpacity(0.5).getStyle('zoom'));
    }
  },
  
  testElementGetStyle: function() {
    this.assertEqual("none",
      $('style_test_1').getStyle('display'));
    
    // not displayed, so "null" ("auto" is tranlated to "null")
    this.assertNull(Element.getStyle('style_test_1', 'width'), 'elements that are hidden should return null on getStyle("width")');
    
    $('style_test_1').show();
    
    // from id rule
    this.assertEqual("pointer",
      Element.getStyle('style_test_1','cursor'));
    
    this.assertEqual("block",
      Element.getStyle('style_test_2','display'));
    
    // we should always get something for width (if displayed)
    // firefox and safari automatically send the correct value,
    // IE is special-cased to do the same
    this.assertEqual($('style_test_2').offsetWidth+'px', Element.getStyle('style_test_2','width'));
    
    this.assertEqual("static",Element.getStyle('style_test_1','position'));
    // from style
    this.assertEqual("11px",
      Element.getStyle('style_test_2','font-size'));
    // from class
    this.assertEqual("1px",
      Element.getStyle('style_test_2','margin-left'));
    
    ['not_floating_none','not_floating_style','not_floating_inline'].each(function(element) {
      this.assertEqual('none', $(element).getStyle('float'));
      this.assertEqual('none', $(element).getStyle('cssFloat'));
    }, this);
    
    ['floating_style','floating_inline'].each(function(element) {
      this.assertEqual('left', $(element).getStyle('float'));
      this.assertEqual('left', $(element).getStyle('cssFloat'));
    }, this);

    this.assertEqual(0.5, $('op1').getStyle('opacity'));
    this.assertEqual(0.5, $('op2').getStyle('opacity'));
    this.assertEqual(1.0, $('op3').getStyle('opacity'));
    
    $('op1').setStyle({opacity: '0.3'});
    $('op2').setStyle({opacity: '0.3'});
    $('op3').setStyle({opacity: '0.3'});
    
    this.assertEqual(0.3, $('op1').getStyle('opacity'));
    this.assertEqual(0.3, $('op2').getStyle('opacity'));
    this.assertEqual(0.3, $('op3').getStyle('opacity'));
    
    $('op3').setStyle({opacity: 0});
    this.assertEqual(0, $('op3').getStyle('opacity'));
    
    if (navigator.appVersion.match(/MSIE/)) {
      this.assertEqual('alpha(opacity=30)', $('op1').getStyle('filter'));
      this.assertEqual('progid:DXImageTransform.Microsoft.Blur(strength=10)alpha(opacity=30)', $('op2').getStyle('filter'));
      $('op2').setStyle({opacity:''});
      this.assertEqual('progid:DXImageTransform.Microsoft.Blur(strength=10)', $('op2').getStyle('filter'));
      this.assertEqual('alpha(opacity=0)', $('op3').getStyle('filter'));
      this.assertEqual(0.3, $('op4-ie').getStyle('opacity'));
    }
    // verify that value is still found when using camelized
    // strings (function previously used getPropertyValue()
    // which expected non-camelized strings)
    this.assertEqual("12px", $('style_test_1').getStyle('fontSize'));
    
    // getStyle on width/height should return values according to
    // the CSS box-model, which doesn't include 
    // margin, padding, or borders
    // TODO: This test fails on IE because there seems to be no way
    // to calculate this properly (clientWidth/Height returns 0)
    if (!navigator.appVersion.match(/MSIE/)) {
      this.assertEqual("14px", $('style_test_dimensions').getStyle('width'));
      this.assertEqual("17px", $('style_test_dimensions').getStyle('height'));
    }
    
    // height/width could always be calculated if it's set to "auto" (Firefox)
    this.assertNotNull($('auto_dimensions').getStyle('height'));
    this.assertNotNull($('auto_dimensions').getStyle('width'));
  },
  
  testElementGetOpacity: function() {
    this.assertEqual(0.45, $('op1').setOpacity(0.45).getOpacity());
  },
  
  testElementReadAttribute: function() {
    var attribFormIssues = $('attributes_with_issues_form');
 	  this.assertEqual('blah-class', attribFormIssues.readAttribute('class'));
 	  this.assertEqual('post', attribFormIssues.readAttribute('method'));
 	  this.assertEqual('string', typeof(attribFormIssues.readAttribute('action')));
 	  this.assertEqual('string', typeof(attribFormIssues.readAttribute('id')));
    
    $(document.body).insert('<div id="ie_href_test_div"></div>'); 
    $('ie_href_test_div').insert('<p>blah blah</p><a id="ie_href_test" href="test.html">blah</a>'); 
    this.assertEqual('test.html', $('ie_href_test').readAttribute('href')); 
    
    this.assertEqual('test.html' , $('attributes_with_issues_1').readAttribute('href'));
    this.assertEqual('L' , $('attributes_with_issues_1').readAttribute('accesskey'));
    this.assertEqual('50' , $('attributes_with_issues_1').readAttribute('tabindex'));
    this.assertEqual('a link' , $('attributes_with_issues_1').readAttribute('title'));
    
    $('cloned_element_attributes_issue').readAttribute('foo')
    var clone = $('cloned_element_attributes_issue').cloneNode(true);
    clone.writeAttribute('foo', 'cloned');
    this.assertEqual('cloned', clone.readAttribute('foo'));
    this.assertEqual('original', $('cloned_element_attributes_issue').readAttribute('foo'));
    
    ['href', 'accesskey', 'accesskey', 'title'].each(function(attr) {
      this.assertEqual('' , $('attributes_with_issues_2').readAttribute(attr));
    }, this);
    
    ['checked','disabled','readonly','multiple'].each(function(attr) {
      this.assertEqual(attr, $('attributes_with_issues_'+attr).readAttribute(attr));
    }, this);
    
    this.assertEqual("alert('hello world');", $('attributes_with_issues_1').readAttribute('onclick'));
    this.assertNull($('attributes_with_issues_1').readAttribute('onmouseover'));
   
    this.assertEqual('date', $('attributes_with_issues_type').readAttribute('type'));
    this.assertEqual('text', $('attributes_with_issues_readonly').readAttribute('type'));
    
    var elements = $('custom_attributes').immediateDescendants();
    this.assertEnumEqual(['1', '2'], elements.invoke('readAttribute', 'foo'));
    this.assertEnumEqual(['2', null], elements.invoke('readAttribute', 'bar'));

    var table = $('write_attribute_table');
    this.assertEqual('4', table.readAttribute('cellspacing'));
    this.assertEqual('6', table.readAttribute('cellpadding'));
    
    // test for consistent flag value across browsers
    ["true", true, " ", 'rEadOnLy'].each(function(value) {
      $('attributes_with_issues_readonly').writeAttribute('readonly', value);
      this.assertEqual('readonly', $('attributes_with_issues_readonly').readAttribute('readonly'));
    }, this);
  },
  
  testElementWriteAttribute: function() {
    var element = Element.extend(document.body.appendChild(document.createElement('p')));
    this.assertRespondsTo('writeAttribute', element);
    this.assertEqual(element, element.writeAttribute('id', 'write_attribute_test'));
    this.assertEqual('write_attribute_test', element.id);
    this.assertEqual('http://prototypejs.org/', $('write_attribute_link').
      writeAttribute({href: 'http://prototypejs.org/', title: 'Home of Prototype'}).href);
    this.assertEqual('Home of Prototype', $('write_attribute_link').title);
    
    var element2 = Element.extend(document.createElement('p'));
    element2.writeAttribute('id', 'write_attribute_without_hash');
    this.assertEqual('write_attribute_without_hash', element2.id);
    element2.writeAttribute('animal', 'cat');
    this.assertEqual('cat', element2.readAttribute('animal'));
  },
  
  testElementWriteAttributeWithBooleans: function() {
    var input = $('write_attribute_input'),
      select = $('write_attribute_select'),
      checkbox = $('write_attribute_checkbox'),
      checkedCheckbox = $('write_attribute_checked_checkbox');
    this.assert( input.          writeAttribute('readonly').            hasAttribute('readonly'));
    this.assert(!input.          writeAttribute('readonly', false).     hasAttribute('readonly'));
    this.assert( input.          writeAttribute('readonly', true).      hasAttribute('readonly'));
    this.assert(!input.          writeAttribute('readonly', null).      hasAttribute('readonly'));
    this.assert( input.          writeAttribute('readonly', 'readonly').hasAttribute('readonly'));
    this.assert( select.         writeAttribute('multiple').            hasAttribute('multiple'));
    this.assert( input.          writeAttribute('disabled').            hasAttribute('disabled'));
    this.assert( checkbox.       writeAttribute('checked').             checked);
    this.assert(!checkedCheckbox.writeAttribute('checked', false).      checked);
  },

  testElementWriteAttributeWithIssues: function() {
    var input = $('write_attribute_input').writeAttribute({maxlength: 90, minlength:80, tabindex: 10}),
      td = $('write_attribute_td').writeAttribute({valign: 'bottom', colspan: 2, rowspan: 2});
    this.assertEqual(80, input.readAttribute('minlength'));
    this.assertEqual(90, input.readAttribute('maxlength'));
    this.assertEqual(10, input.readAttribute('tabindex'));
    this.assertEqual(2,  td.readAttribute('colspan'));
    this.assertEqual(2,  td.readAttribute('rowspan'));
    this.assertEqual('bottom', td.readAttribute('valign'));
    
    var p = $('write_attribute_para'), label = $('write_attribute_label');
    this.assertEqual('some-class',     p.    writeAttribute({'class':   'some-class'}).    readAttribute('class'));
    this.assertEqual('some-className', p.    writeAttribute({className: 'some-className'}).readAttribute('class'));
    this.assertEqual('some-id',        label.writeAttribute({'for':     'some-id'}).       readAttribute('for'));
    this.assertEqual('some-other-id',  label.writeAttribute({htmlFor:   'some-other-id'}). readAttribute('for'));
    
    this.assert(p.writeAttribute({style: 'width: 5px;'}).readAttribute('style').toLowerCase().include('width'));      

    var table = $('write_attribute_table');
    table.writeAttribute('cellspacing', '2')
    table.writeAttribute('cellpadding', '3')
    this.assertEqual('2', table.readAttribute('cellspacing'));
    this.assertEqual('3', table.readAttribute('cellpadding'));

    var iframe = new Element('iframe', { frameborder: 0 });
    this.assertIdentical(0, parseInt(iframe.readAttribute('frameborder')));
  },
  
  testElementWriteAttributeWithCustom: function() {
    var p = $('write_attribute_para').writeAttribute({name: 'martin', location: 'stockholm', age: 26});
    this.assertEqual('martin',    p.readAttribute('name'));
    this.assertEqual('stockholm', p.readAttribute('location'));
    this.assertEqual('26',        p.readAttribute('age'));
  },
  
  testElementHasAttribute: function() {
    var label = $('write_attribute_label');
    this.assertIdentical(true,  label.hasAttribute('for'));
    this.assertIdentical(false, label.hasAttribute('htmlFor'));
    this.assertIdentical(false, label.hasAttribute('className'));
    this.assertIdentical(false, label.hasAttribute('rainbows'));
    
    var input = $('write_attribute_input');
    this.assertNotIdentical(null, input.hasAttribute('readonly'));
    this.assertNotIdentical(null, input.hasAttribute('readOnly'));
  },
  
  testNewElement: function() {
    this.assert(new Element('h1'));
    
    var XHTML_TAGS = $w(
      'a abbr acronym address area '+
      'b bdo big blockquote br button caption '+
      'cite code col colgroup dd del dfn div dl dt '+
      'em fieldset form h1 h2 h3 h4 h5 h6 hr '+
      'i iframe img input ins kbd label legend li '+
      'map object ol optgroup option p param pre q samp '+
      'script select small span strong style sub sup '+
      'table tbody td textarea tfoot th thead tr tt ul var');
      
    XHTML_TAGS.each(function(tag, index) {
      var id = tag + '_' + index, element = document.body.appendChild(new Element(tag, {id: id}));
      this.assertEqual(tag, element.tagName.toLowerCase());
      this.assertEqual(element, document.body.lastChild);
      this.assertEqual(id, element.id);
    }, this);
    
    
    this.assertRespondsTo('update', new Element('div'));
    Element.addMethods({
      cheeseCake: function(){
        return 'Cheese cake';
      }
    });
    
    this.assertRespondsTo('cheeseCake', new Element('div'));
    
    /* window.ElementOld = function(tagName, attributes) { 
      if (Prototype.Browser.IE && attributes && attributes.name) { 
        tagName = '<' + tagName + ' name="' + attributes.name + '">'; 
        delete attributes.name; 
      } 
      return Element.extend(document.createElement(tagName)).writeAttribute(attributes || {}); 
    };
    
    this.benchmark(function(){
      XHTML_TAGS.each(function(tagName) { new Element(tagName) });
    }, 5);
    
    this.benchmark(function(){
      XHTML_TAGS.each(function(tagName) { new ElementOld(tagName) });
    }, 5); */
    
    this.assertEqual('foobar', new Element('a', {custom: 'foobar'}).readAttribute('custom'));
    var input = document.body.appendChild(new Element('input', 
      {id: 'my_input_field_id', name: 'my_input_field'}));
    this.assertEqual(input, document.body.lastChild);
    this.assertEqual('my_input_field', $(document.body.lastChild).name);
    if (Prototype.Browser.IE)
      this.assertMatch(/name=["']?my_input_field["']?/, $('my_input_field').outerHTML);
    
    if (originalElement && Prototype.BrowserFeatures.ElementExtensions) {
      Element.prototype.fooBar = Prototype.emptyFunction
      this.assertRespondsTo('fooBar', new Element('div'));
    }
    
    //test IE setting "type" property of newly created button element
    var button = new Element('button', {id:'button_type_test',type: 'reset'}); 
 	var form   = $('attributes_with_issues_form');   
 	var input  = $('attributes_with_issues_regular');    
 	
 	form.insert(button); 
 	input.value = 1; 
 	button.click();
 	
 	this.assertEqual('0', input.value);
 	button.remove();
  },

  testElementGetHeight: function() {
    this.assertIdentical(100, $('dimensions-visible').getHeight());
    this.assertIdentical(100, $('dimensions-display-none').getHeight());
  },
  
  testElementGetWidth: function() {
    this.assertIdentical(200, $('dimensions-visible').getWidth());
    this.assertIdentical(200, $('dimensions-display-none').getWidth());
  },
  
  testElementGetDimensions: function() {
    this.assertIdentical(100, $('dimensions-visible').getDimensions().height);
    this.assertIdentical(200, $('dimensions-visible').getDimensions().width);
    this.assertIdentical(100, $('dimensions-display-none').getDimensions().height);
    this.assertIdentical(200, $('dimensions-display-none').getDimensions().width);
    
    this.assertIdentical(100, $('dimensions-visible-pos-rel').getDimensions().height);
    this.assertIdentical(200, $('dimensions-visible-pos-rel').getDimensions().width);
    this.assertIdentical(100, $('dimensions-display-none-pos-rel').getDimensions().height);
    this.assertIdentical(200, $('dimensions-display-none-pos-rel').getDimensions().width);
    
    this.assertIdentical(100, $('dimensions-visible-pos-abs').getDimensions().height);
    this.assertIdentical(200, $('dimensions-visible-pos-abs').getDimensions().width);
    this.assertIdentical(100, $('dimensions-display-none-pos-abs').getDimensions().height);
    this.assertIdentical(200, $('dimensions-display-none-pos-abs').getDimensions().width);
    
    // known failing issue
    // this.assert($('dimensions-nestee').getDimensions().width <= 500, 'check for proper dimensions of hidden child elements');
    
    $('dimensions-td').hide();
    this.assertIdentical(100, $('dimensions-td').getDimensions().height);
    this.assertIdentical(200, $('dimensions-td').getDimensions().width);
    $('dimensions-td').show();
    
    $('dimensions-tr').hide();
    this.assertIdentical(100, $('dimensions-tr').getDimensions().height);
    this.assertIdentical(200, $('dimensions-tr').getDimensions().width);
    $('dimensions-tr').show();
    
    $('dimensions-table').hide();
    this.assertIdentical(100, $('dimensions-table').getDimensions().height);
    this.assertIdentical(200, $('dimensions-table').getDimensions().width);
    
  },
      
  testDOMAttributesHavePrecedenceOverExtendedElementMethods: function() {
    this.assertNothingRaised(function() { $('dom_attribute_precedence').down('form') });
    this.assertEqual($('dom_attribute_precedence').down('input'), $('dom_attribute_precedence').down('form').update);
  },
  
  testClassNames: function() {
    this.assertEnumEqual([], $('class_names').classNames());
    this.assertEnumEqual(['A'], $('class_names').down().classNames());
    this.assertEnumEqual(['A', 'B'], $('class_names_ul').classNames());
  },
  
  testHasClassName: function() {
    this.assertIdentical(false, $('class_names').hasClassName('does_not_exist'));
    this.assertIdentical(true, $('class_names').down().hasClassName('A'));
    this.assertIdentical(false, $('class_names').down().hasClassName('does_not_exist'));
    this.assertIdentical(true, $('class_names_ul').hasClassName('A'));
    this.assertIdentical(true, $('class_names_ul').hasClassName('B'));
    this.assertIdentical(false, $('class_names_ul').hasClassName('does_not_exist'));
  },
  
  testAddClassName: function() {
    $('class_names').addClassName('added_className');
    this.assertEnumEqual(['added_className'], $('class_names').classNames());
    
    $('class_names').addClassName('added_className'); // verify that className cannot be added twice.
    this.assertEnumEqual(['added_className'], $('class_names').classNames());
    
    $('class_names').addClassName('another_added_className');
    this.assertEnumEqual(['added_className', 'another_added_className'], $('class_names').classNames());
  },
  
  testRemoveClassName: function() {
    $('class_names').removeClassName('added_className');
    this.assertEnumEqual(['another_added_className'], $('class_names').classNames());
    
    $('class_names').removeClassName('added_className'); // verify that removing a non existent className is safe.
    this.assertEnumEqual(['another_added_className'], $('class_names').classNames());
    
    $('class_names').removeClassName('another_added_className');
    this.assertEnumEqual([], $('class_names').classNames());
  },
  
  testToggleClassName: function() {
    $('class_names').toggleClassName('toggled_className');
    this.assertEnumEqual(['toggled_className'], $('class_names').classNames());
    
    $('class_names').toggleClassName('toggled_className');
    this.assertEnumEqual([], $('class_names').classNames());
    
    $('class_names_ul').toggleClassName('toggled_className');
    this.assertEnumEqual(['A', 'B', 'toggled_className'], $('class_names_ul').classNames());
           
    $('class_names_ul').toggleClassName('toggled_className');
    this.assertEnumEqual(['A', 'B'], $('class_names_ul').classNames());  
  },
  
  testElementScrollTo: function() {
    var elem = $('scroll_test_2');
    Element.scrollTo('scroll_test_2');
    this.assertEqual(Position.page(elem)[1], 0);
    window.scrollTo(0, 0);
    
    elem.scrollTo();
    this.assertEqual(Position.page(elem)[1], 0);      
    window.scrollTo(0, 0);
  },
  
  testCustomElementMethods: function() {
    var elem = $('navigation_test_f');
    this.assertRespondsTo('hashBrowns', elem);
    this.assertEqual('hash browns', elem.hashBrowns());
    
    this.assertRespondsTo('hashBrowns', Element);
    this.assertEqual('hash browns', Element.hashBrowns(elem));
  },
  
  testSpecificCustomElementMethods: function() {
    var elem = $('navigation_test_f');
    
    this.assert(Element.Methods.ByTag[elem.tagName]);
    this.assertRespondsTo('pancakes', elem);
    this.assertEqual("pancakes", elem.pancakes());
    
    var elem2 = $('test-visible');

    this.assert(Element.Methods.ByTag[elem2.tagName]);
    this.assertUndefined(elem2.pancakes);
    this.assertRespondsTo('waffles', elem2);
    this.assertEqual("waffles", elem2.waffles());
    
    this.assertRespondsTo('orangeJuice', elem);
    this.assertRespondsTo('orangeJuice', elem2);
    this.assertEqual("orange juice", elem.orangeJuice());
    this.assertEqual("orange juice", elem2.orangeJuice());
    
    this.assert(typeof Element.orangeJuice == 'undefined');
    this.assert(typeof Element.pancakes == 'undefined');
    this.assert(typeof Element.waffles == 'undefined');
    
  },
  
  testScriptFragment: function() {
    var element = document.createElement('div');
    // tests an issue with Safari 2.0 crashing when the ScriptFragment
    // regular expression is using a pipe-based approach for
    // matching any character
    ['\r','\n',' '].each(function(character){
      $(element).update("<script>"+character.times(10000)+"</scr"+"ipt>");
      this.assertEqual('', element.innerHTML);
    }, this);
    $(element).update("<script>var blah='"+'\\'.times(10000)+"'</scr"+"ipt>");
    this.assertEqual('', element.innerHTML);
  },

  testPositionedOffset: function() {
    this.assertEnumEqual([10,10],
      $('body_absolute').positionedOffset());
    this.assertEnumEqual([10,10],
      $('absolute_absolute').positionedOffset());
    this.assertEnumEqual([10,10],
      $('absolute_relative').positionedOffset());
    this.assertEnumEqual([0,10],
      $('absolute_relative_undefined').positionedOffset());
    this.assertEnumEqual([10,10],
      $('absolute_fixed_absolute').positionedOffset());
      
    var afu = $('absolute_fixed_undefined');
    this.assertEnumEqual([afu.offsetLeft, afu.offsetTop],
      afu.positionedOffset());
      
    var element = new Element('div'), offset = element.positionedOffset();
    this.assertEnumEqual([0,0], offset);
    this.assertIdentical(0, offset.top);
    this.assertIdentical(0, offset.left);
  },
  
  testCumulativeOffset: function() {
    var element = new Element('div'), offset = element.cumulativeOffset();
    this.assertEnumEqual([0,0], offset);
    this.assertIdentical(0, offset.top);
    this.assertIdentical(0, offset.left);
  },
  
  testViewportOffset: function() {
    this.assertEnumEqual([10,10],
      $('body_absolute').viewportOffset());
    this.assertEnumEqual([20,20],
      $('absolute_absolute').viewportOffset());
    this.assertEnumEqual([20,20],
      $('absolute_relative').viewportOffset());
    this.assertEnumEqual([20,30],
      $('absolute_relative_undefined').viewportOffset());
    var element = new Element('div'), offset = element.viewportOffset();
    this.assertEnumEqual([0,0], offset);
    this.assertIdentical(0, offset.top);
    this.assertIdentical(0, offset.left);
  },
  
  testOffsetParent: function() {
    this.assertEqual('body_absolute', $('absolute_absolute').getOffsetParent().id);
    this.assertEqual('body_absolute', $('absolute_relative').getOffsetParent().id);
    this.assertEqual('absolute_relative', $('inline').getOffsetParent().id);
    this.assertEqual('absolute_relative', $('absolute_relative_undefined').getOffsetParent().id);
    
    this.assertEqual(document.body, new Element('div').getOffsetParent());
  },

  testAbsolutize: function() {
    $('notInlineAbsoluted', 'inlineAbsoluted').each(function(elt) {
      if ('_originalLeft' in elt) delete elt._originalLeft;
      elt.absolutize();
      this.assertUndefined(elt._originalLeft, 'absolutize() did not detect absolute positioning');
    }, this);
    // invoking on "absolute" positioned element should return element 
    var element = $('absolute_fixed_undefined').setStyle({position: 'absolute'});
    this.assertEqual(element, element.absolutize());
    
    // test relatively positioned element with no height specified for IE7
    var element = $('absolute_relative'), dimensions = element.getDimensions();
    element.absolutize();
    this.assertIdentical(dimensions.width, element.getDimensions().width);
    this.assertIdentical(dimensions.height, element.getDimensions().height);
  },
  
  testRelativize: function() {
    // invoking on "relative" positioned element should return element
    var element = $('absolute_fixed_undefined').setStyle({position: 'relative'});
    this.assertEqual(element, element.relativize());
    
    var assertPositionEqual = function(modifier, element) {
      element = $(element);
      var offsets = element.cumulativeOffset();
      Element[modifier](element);
      this.assertEnumEqual(offsets, element.cumulativeOffset());
    }.bind(this);

    var testRelativize = assertPositionEqual.curry('relativize');
    testRelativize('notInlineAbsoluted');
    testRelativize('inlineAbsoluted');
    testRelativize('absolute_absolute');
  },
  
  testElementToViewportDimensionsDoesNotAffectDocumentProperties: function() {
    // No properties on the document should be affected when resizing
    // an absolute positioned(0,0) element to viewport dimensions
    var vd = document.viewport.getDimensions();

    var before = documentViewportProperties.inspect();
    $('elementToViewportDimensions').setStyle({ height: vd.height + 'px', width: vd.width + 'px' }).show();
    var after = documentViewportProperties.inspect();
    $('elementToViewportDimensions').hide();

    documentViewportProperties.properties.each(function(prop) {
      this.assertEqual(before[prop], after[prop], prop + ' was affected');
    }, this);
  },
  
  testNodeConstants: function() {
    this.assert(window.Node, 'window.Node is unavailable');

    var constants = $H({
      ELEMENT_NODE: 1,
      ATTRIBUTE_NODE: 2,
      TEXT_NODE: 3,
      CDATA_SECTION_NODE: 4,
      ENTITY_REFERENCE_NODE: 5,
      ENTITY_NODE: 6,
      PROCESSING_INSTRUCTION_NODE: 7,
      COMMENT_NODE: 8,
      DOCUMENT_NODE: 9,
      DOCUMENT_TYPE_NODE: 10,
      DOCUMENT_FRAGMENT_NODE: 11,
      NOTATION_NODE: 12
    });

    constants.each(function(pair) {
      this.assertEqual(Node[pair.key], pair.value);
    }, this);
  }
});

function preservingBrowserDimensions(callback) {
  var original = document.viewport.getDimensions();
  window.resizeTo(640, 480);
  var resized = document.viewport.getDimensions();
  original.width += 640 - resized.width, original.height += 480 - resized.height;
  
  try {
    window.resizeTo(original.width, original.height);
    callback();
  } finally {
    window.resizeTo(original.width, original.height);
  }
}
