
INSERT_TESTS = {
  'id':         'I',
  'caption':    'Insert Tests',
  'checkAttrs': False,
  'checkStyle': False,
  
  'Proposed': [
    { 'desc':       '',
      'command':    '',
      'tests':      [
      ]
    },

    { 'desc':       'insert <hr>',
      'command':    'inserthorizontalrule',
      'tests':      [
        { 'id':         'IHR_TEXT-1_SC',
          'rte1-id':    'a-inserthorizontalrule-0',
          'desc':       'Insert <hr> into text',
          'pad':        'foo^bar',
          'expected':   'foo<hr>^bar',
          'accept':     'foo<hr>|bar' },

        { 'id':         'IHR_TEXT-1_SI',
          'desc':       'Insert <hr>, replacing selected text',
          'pad':        'foo[bar]baz',
          'expected':   'foo<hr>^baz',
          'accept':     'foo<hr>|baz' },

        { 'id':         'IHR_DIV-B-1_SX',
          'desc':       'Insert <hr> between elements',
          'pad':        '<div><b>foo</b>|<b>bar</b></div>',
          'expected':   '<div><b>foo</b><hr>|<b>bar</b></div>' },

        { 'id':         'IHR_DIV-B-2_SO',
          'desc':       'Insert <hr>, replacing a fully wrapped element',
          'pad':        '<div><b>foo</b>{<b>bar</b>}<b>baz</b></div>',
          'expected':   '<div><b>foo</b><hr>|<b>baz</b></div>' },

        { 'id':         'IHR_B-1_SC',
          'desc':       'Insert <hr> into a span, splitting it',
          'pad':        '<b>foo^bar</b>',
          'expected':   '<b>foo</b><hr><b>^bar</b>' },

        { 'id':         'IHR_B-1_SS',
          'desc':       'Insert <hr> into a span at the start (should not create an empty span)',
          'pad':        '<b>^foobar</b>',
          'expected':   '<hr><b>^foobar</b>' },

        { 'id':         'IHR_B-1_SE',
          'desc':       'Insert <hr> into a span at the end',
          'pad':        '<b>foobar^</b>',
          'expected':   [ '<b>foobar</b><hr>|',
                          '<b>foobar</b><hr><b>^</b>' ] },

        { 'id':         'IHR_B-2_SL',
          'desc':       'Insert <hr> with oblique selection starting outside of span',
          'pad':        'foo[bar<b>baz]qoz</b>',
          'expected':   'foo<hr>|<b>qoz</b>' },

        { 'id':         'IHR_B-2_SLR',
          'desc':       'Insert <hr> with oblique reversed selection starting outside of span',
          'pad':        'foo]bar<b>baz[qoz</b>',
          'expected':   [ 'foo<hr>|<b>qoz</b>',
                          'foo<hr><b>^qoz</b>' ] },

        { 'id':         'IHR_B-3_SR',
          'desc':       'Insert <hr> with oblique selection ending outside of span',
          'pad':        '<b>foo[bar</b>baz]quoz',
          'expected':   [ '<b>foo</b><hr>|quoz',
                          '<b>foo</b><hr><b>^</b>quoz' ] },

        { 'id':         'IHR_B-3_SRR',
          'desc':       'Insert <hr> with oblique reversed selection starting outside of span',
          'pad':        '<b>foo]bar</b>baz[quoz',
          'expected':   '<b>foo</b><hr>|quoz' },

        { 'id':         'IHR_B-I-1_SM',
          'desc':       'Insert <hr> with oblique selection between different spans',
          'pad':        '<b>foo[bar</b><i>baz]quoz</i>',
          'expected':   [ '<b>foo</b><hr>|<i>quoz</i>',
                          '<b>foo</b><hr><b>^</b><i>quoz</i>' ] },

        { 'id':         'IHR_B-I-1_SMR',
          'desc':       'Insert <hr> with reversed oblique selection between different spans',
          'pad':        '<b>foo]bar</b><i>baz[quoz</i>',
          'expected':   '<b>foo</b><hr><i>^quoz</i>' },

        { 'id':         'IHR_P-1_SC',
          'desc':       'Insert <hr> into a paragraph, splitting it',
          'pad':        '<p>foo^bar</p>',
          'expected':   [ '<p>foo</p><hr>|<p>bar</p>',
                          '<p>foo</p><hr><p>^bar</p>' ] },

        { 'id':         'IHR_P-1_SS',
          'desc':       'Insert <hr> into a paragraph at the start (should not create an empty span)',
          'pad':        '<p>^foobar</p>',
          'expected':   [ '<hr>|<p>foobar</p>',
                          '<hr><p>^foobar</p>' ] },

        { 'id':         'IHR_P-1_SE',
          'desc':       'Insert <hr> into a paragraph at the end (should not create an empty span)',
          'pad':        '<p>foobar^</p>',
          'expected':   '<p>foobar</p><hr>|' }
      ]
    },

    { 'desc':       'insert <p>',
      'command':    'insertparagraph',
      'tests':      [
        { 'id':         'IP_P-1_SC',
          'desc':       'Split paragraph',
          'pad':        '<p>foo^bar</p>',
          'expected':   '<p>foo</p><p>^bar</p>' },

        { 'id':         'IP_UL-LI-1_SC',
          'desc':       'Split list item',
          'pad':        '<ul><li>foo^bar</li></ul>',
          'expected':   '<ul><li>foo</li><li>^bar</li></ul>' }
      ]
    },

    { 'desc':       'insert text',
      'command':    'inserttext',
      'tests':      [
        { 'id':         'ITEXT:text_TEXT-1_SC',
          'desc':       'Insert text',
          'value':      'text',
          'pad':        'foo^bar',
          'expected':   'footext^bar' },

        { 'id':         'ITEXT:text_TEXT-1_SI',
          'desc':       'Insert text, replacing selected text',
          'value':      'text',
          'pad':        'foo[bar]baz',
          'expected':   'footext^baz' }
      ]
    },

    { 'desc':       'insert <br>',
      'command':    'insertlinebreak',
      'tests':      [
        { 'id':         'IBR_TEXT-1_SC',
          'desc':       'Insert <br> into text',
          'pad':        'foo^bar',
          'expected':   [ 'foo<br>|bar',
                          'foo<br>^bar' ] },

        { 'id':         'IBR_TEXT-1_SI',
          'desc':       'Insert <br>, replacing selected text',
          'pad':        'foo[bar]baz',
          'expected':   [ 'foo<br>|baz',
                          'foo<br>^baz' ] },

        { 'id':         'IBR_LI-1_SC',
          'desc':       'Insert <br> within list item',
          'pad':        '<ul><li>foo^bar</li></ul>',
          'expected':   '<ul><li>foo<br>^bar</li></ul>' }
      ]
    },

    { 'desc':       'insert <img>',
      'command':    'insertimage',
      'tests':      [
        { 'id':         'IIMG:url_TEXT-1_SC',
          'rte1-id':    'a-insertimage-0',
          'desc':       'Insert image with URL "bar.png"',
          'value':      'bar.png',
          'checkAttrs':  True,
          'pad':        'foo^bar',
          'expected':   [ 'foo<img src="bar.png">|bar',
                          'foo<img src="bar.png">^bar' ] },

        { 'id':         'IIMG:url_IMG-1_SO',
          'desc':       'Change existing image to new URL, selection on <img>',
          'value':      'quz.png',
          'checkAttrs':  True,
          'pad':        '<span>foo{<img src="bar.png">}bar</span>',
          'expected':   [ '<span>foo<img src="quz.png"/>|bar</span>',
                          '<span>foo<img src="quz.png"/>^bar</span>' ] },

        { 'id':         'IIMG:url_SPAN-IMG-1_SO',
          'desc':       'Change existing image to new URL, selection in text surrounding <img>',
          'value':      'quz.png',
          'checkAttrs':  True,
          'pad':        'foo[<img src="bar.png">]bar',
          'expected':   [ 'foo<img src="quz.png"/>|bar',
                          'foo<img src="quz.png"/>^bar' ] },

        { 'id':         'IIMG:._SPAN-IMG-1_SO',
          'desc':       'Remove existing image or URL, selection on <img>',
          'value':      '',
          'checkAttrs':  True,
          'pad':        '<span>foo{<img src="bar.png">}bar</span>',
          'expected':   [ '<span>foo^bar</span>',
                          '<span>foo<img>|bar</span>',
                          '<span>foo<img>^bar</span>',
                          '<span>foo<img src="">|bar</span>',
                          '<span>foo<img src="">^bar</span>' ] },

        { 'id':         'IIMG:._IMG-1_SO',
          'desc':       'Remove existing image or URL, selection in text surrounding <img>',
          'value':      '',
          'checkAttrs':  True,
          'pad':        'foo[<img src="bar.png">]bar',
          'expected':   [ 'foo^bar',
                          'foo<img>|bar',
                          'foo<img>^bar',
                          'foo<img src="">|bar',
                          'foo<img src="">^bar' ] }
      ]
    },

    { 'desc':       'insert <ol>',
      'command':    'insertorderedlist',
      'tests':      [
        { 'id':         'IOL_TEXT-1_SC',
          'rte1-id':    'a-insertorderedlist-0',
          'desc':       'Insert ordered list on collapsed selection',
          'pad':        'foo^bar',
          'expected':   '<ol><li>foo^bar</li></ol>' },

        { 'id':         'IOL_TEXT-1_SI',
          'desc':       'Insert ordered list on selected text',
          'pad':        'foo[bar]baz',
          'expected':   '<ol><li>foo[bar]baz</li></ol>' }
      ]
    },

    { 'desc':       'insert <ul>',
      'command':    'insertunorderedlist',
      'tests':      [
        { 'id':         'IUL_TEXT-1_SC',
          'desc':       'Insert unordered list on collapsed selection',
          'pad':        'foo^bar',
          'expected':   '<ul><li>foo^bar</li></ul>' },

        { 'id':         'IUL_TEXT-1_SI',
          'rte1-id':    'a-insertunorderedlist-0',
          'desc':       'Insert unordered list on selected text',
          'pad':        'foo[bar]baz',
          'expected':   '<ul><li>foo[bar]baz</li></ul>' }
      ]
    },

    { 'desc':       'insert arbitrary HTML',
      'command':    'inserthtml',
      'tests':      [
        { 'id':         'IHTML:BR_TEXT-1_SC',
          'rte1-id':    'a-inserthtml-0',
          'desc':       'InsertHTML: <br>',
          'value':      '<br>',
          'pad':        'foo^barbaz',
          'expected':   'foo<br>^barbaz' },

        { 'id':         'IHTML:text_TEXT-1_SI',
          'desc':       'InsertHTML: "NEW"',
          'value':      'NEW',
          'pad':        'foo[bar]baz',
          'expected':   'fooNEW^baz' },

        { 'id':         'IHTML:S_TEXT-1_SI',
          'desc':       'InsertHTML: "<span>NEW<span>"',
          'value':      '<span>NEW</span>',
          'pad':        'foo[bar]baz',
          'expected':   'foo<span>NEW</span>^baz' },

        { 'id':         'IHTML:H1.H2_TEXT-1_SI',
          'desc':       'InsertHTML: "<h1>NEW</h1><h2>HTML</h2>"',
          'value':      '<h1>NEW</h1><h2>HTML</h2>',
          'pad':        'foo[bar]baz',
          'expected':   'foo<h1>NEW</h1><h2>HTML</h2>^baz' },

        { 'id':         'IHTML:P-B_TEXT-1_SI',
          'desc':       'InsertHTML: "<p>NEW<b>HTML</b>!</p>"',
          'value':      '<p>NEW<b>HTML</b>!</p>',
          'pad':        'foo[bar]baz',
          'expected':   'foo<p>NEW<b>HTML</b>!</p>^baz' }
      ]
    }
  ]
}


