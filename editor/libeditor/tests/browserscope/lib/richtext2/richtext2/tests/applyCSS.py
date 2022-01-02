
APPLY_TESTS_CSS = {
  'id':            'AC',
  'caption':       'Apply Formatting Tests, using styleWithCSS',
  'checkAttrs':    True,
  'checkStyle':    True,
  'styleWithCSS':  True,

  'Proposed': [
    { 'desc':       '',
      'command':    '',
      'tests':      [
      ]
    },

    { 'desc':       '[HTML5] bold',
      'command':    'bold',
      'tests':      [
        { 'id':         'B_TEXT-1_SI',
          'rte1-id':    'a-bold-1',
          'desc':       'Bold selection',
          'pad':        'foo[bar]baz',
          'expected':   'foo<span style="font-weight: bold">[bar]</span>baz' }
      ]
    },

    { 'desc':       '[HTML5] italic',
      'command':    'italic',
      'tests':      [
        { 'id':         'I_TEXT-1_SI',
          'rte1-id':    'a-italic-1',
          'desc':       'Italicize selection',
          'pad':        'foo[bar]baz',
          'expected':   'foo<span style="font-style: italic">[bar]</span>baz' }
      ]
    },

    { 'desc':       '[HTML5] underline',
      'command':    'underline',
      'tests':      [
        { 'id':         'U_TEXT-1_SI',
          'rte1-id':    'a-underline-1',
          'desc':       'Underline selection',
          'pad':        'foo[bar]baz',
          'expected':   'foo<span style="text-decoration: underline">[bar]</span>baz' }
      ]
    },

    { 'desc':       '[HTML5] strikethrough',
      'command':    'strikethrough',
      'tests':      [
        { 'id':         'S_TEXT-1_SI',
          'rte1-id':    'a-strikethrough-1',
          'desc':       'Strike-through selection',
          'pad':        'foo[bar]baz',
          'expected':   'foo<span style="text-decoration: line-through">[bar]</span>baz' }
      ]
    },

    { 'desc':       '[HTML5] subscript',
      'command':    'subscript',
      'tests':      [
        { 'id':         'SUB_TEXT-1_SI',
          'rte1-id':    'a-subscript-1',
          'desc':       'Change selection to subscript',
          'pad':        'foo[bar]baz',
          'expected':   'foo<span style="vertical-align: sub">[bar]</span>baz' }
      ]
    },

    { 'desc':       '[HTML5] superscript',
      'command':    'superscript',
      'tests':      [
        { 'id':         'SUP_TEXT-1_SI',
          'rte1-id':    'a-superscript-1',
          'desc':       'Change selection to superscript',
          'pad':        'foo[bar]baz',
          'expected':   'foo<span style="vertical-align: super">[bar]</span>baz' }
      ]
    },


    { 'desc':       '[MIDAS] backcolor',
      'command':    'backcolor',
      'tests':      [
        { 'id':         'BC:blue_TEXT-1_SI',
          'rte1-id':    'a-backcolor-1',
          'desc':       'Change background color',
          'value':      'blue',
          'pad':        'foo[bar]baz',
          'expected':   [ 'foo<span style="background-color: blue">[bar]</span>baz',
                          'foo<font style="background-color: blue">[bar]</font>baz' ] }
      ]
    },

    { 'desc':       '[MIDAS] forecolor',
      'command':    'forecolor',
      'tests':      [
        { 'id':         'FC:blue_TEXT-1_SI',
          'rte1-id':    'a-forecolor-1',
          'desc':       'Change the text color',
          'value':      'blue',
          'pad':        'foo[bar]baz',
          'expected':   [ 'foo<span style="color: blue">[bar]</span>baz',
                          'foo<font style="color: blue">[bar]</font>baz' ] }
      ]
    },

    { 'desc':       '[MIDAS] hilitecolor',
      'command':    'hilitecolor',
      'tests':      [
        { 'id':         'HC:blue_TEXT-1_SI',
          'rte1-id':    'a-hilitecolor-1',
          'desc':       'Change the hilite color',
          'value':      'blue',
          'pad':        'foo[bar]baz',
          'expected':   [ 'foo<span style="background-color: blue">[bar]</span>baz',
                          'foo<font style="background-color: blue">[bar]</font>baz' ] }
      ]
    },

    { 'desc':       '[MIDAS] fontname',
      'command':    'fontname',
      'tests':      [
        { 'id':         'FN:a_TEXT-1_SI',
          'rte1-id':    'a-fontname-1',
          'desc':       'Change the font name',
          'value':      'arial',
          'pad':        'foo[bar]baz',
          'expected':   [ 'foo<span style="font-family: arial">[bar]</span>baz',
                          'foo<font style="font-family: blue">[bar]</font>baz' ] }
      ]
    },

    { 'desc':       '[MIDAS] fontsize',
      'command':    'fontsize',
      'tests':      [
        { 'id':         'FS:2_TEXT-1_SI',
          'rte1-id':    'a-fontsize-1',
          'desc':       'Change the font size to "2"',
          'value':      '2',
          'pad':        'foo[bar]baz',
          'expected':   [ 'foo<span style="font-size: small">[bar]</span>baz',
                          'foo<font style="font-size: small">[bar]</font>baz' ] },

        { 'id':         'FS:18px_TEXT-1_SI',
          'desc':       'Change the font size to "18px"',
          'value':      '18px',
          'pad':        'foo[bar]baz',
          'expected':   [ 'foo<span style="font-size: 18px">[bar]</span>baz',
                          'foo<font style="font-size: 18px">[bar]</font>baz' ] },

        { 'id':         'FS:large_TEXT-1_SI',
          'desc':       'Change the font size to "large"',
          'value':      'large',
          'pad':        'foo[bar]baz',
          'expected':   [ 'foo<span style="font-size: large">[bar]</span>baz',
                          'foo<font style="font-size: large">[bar]</font>baz' ] }
      ]
    },

    { 'desc':       '[MIDAS] indent',
      'command':    'indent',
      'tests':      [
        { 'id':         'IND_TEXT-1_SI',
          'rte1-id':    'a-indent-1',
          'desc':       'Indent the text (assume "standard" 40px)',
          'pad':        'foo[bar]baz',
          'expected':   [ '<div style="margin-left: 40px">foo[bar]baz</div>',
                          '<div style="margin: 0 0 0 40px">foo[bar]baz</div>',
                          '<blockquote style="margin-left: 40px">foo[bar]baz</blockquote>',
                          '<blockquote style="margin: 0 0 0 40px">foo[bar]baz</blockquote>' ],
          'div': {
            'accOuter': [ '<div contenteditable="true" style="margin-left: 40px">foo[bar]baz</div>',
                          '<div contenteditable="true" style="margin: 0 0 0 40px">foo[bar]baz</div>' ] } }
      ]
    },

    { 'desc':       '[MIDAS] outdent (-> unapply tests)',
      'command':    'outdent',
      'tests':      [
      ]
    },

    { 'desc':       '[MIDAS] justifycenter',
      'command':    'justifycenter',
      'tests':      [
        { 'id':         'JC_TEXT-1_SC',
          'rte1-id':    'a-justifycenter-1',
          'desc':       'justify the text centrally',
          'pad':        'foo^bar',
          'expected':   [ '<p style="text-align: center">foo^bar</p>',
                          '<div style="text-align: center">foo^bar</div>' ],
          'div': {
            'accOuter': '<div contenteditable="true" style="text-align: center">foo^bar</div>' } }
      ]
    },

    { 'desc':       '[MIDAS] justifyfull',
      'command':    'justifyfull',
      'tests':      [
        { 'id':         'JF_TEXT-1_SC',
          'rte1-id':    'a-justifyfull-1',
          'desc':       'justify the text fully',
          'pad':        'foo^bar',
          'expected':   [ '<p style="text-align: justify">foo^bar</p>',
                          '<div style="text-align: justify">foo^bar</div>' ],
          'div': {
            'accOuter': '<div contenteditable="true" style="text-align: justify">foo^bar</div>' } }
      ]
    },

    { 'desc':       '[MIDAS] justifyleft',
      'command':    'justifyleft',
      'tests':      [
        { 'id':         'JL_TEXT-1_SC',
          'rte1-id':    'a-justifyleft-1',
          'desc':       'justify the text left',
          'pad':        'foo^bar',
          'expected':   [ '<p style="text-align: left">foo^bar</p>',
                          '<div style="text-align: left">foo^bar</div>' ],
          'div': {
            'accOuter': '<div contenteditable="true" style="text-align: left">foo^bar</div>' } }
      ]
    },

    { 'desc':       '[MIDAS] justifyright',
      'command':    'justifyright',
      'tests':      [
        { 'id':         'JR_TEXT-1_SC',
          'rte1-id':    'a-justifyright-1',
          'desc':       'justify the text right',
          'pad':        'foo^bar',
          'expected':   [ '<p style="text-align: right">foo^bar</p>',
                          '<div style="text-align: right">foo^bar</div>' ],
          'div': {
            'accOuter': '<div contenteditable="true" style="text-align: right">foo^bar</div>' } }
      ]
    }
  ]
}



