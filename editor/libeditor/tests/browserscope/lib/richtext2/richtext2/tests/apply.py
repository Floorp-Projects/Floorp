
APPLY_TESTS = {
  'id':            'A',
  'caption':       'Apply Formatting Tests',
  'checkAttrs':    True,
  'checkStyle':    True,
  'styleWithCSS':  False,

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
          'rte1-id':    'a-bold-0',
          'desc':       'Bold selection',
          'pad':        'foo[bar]baz',
          'expected':   [ 'foo<b>[bar]</b>baz',
                          'foo<strong>[bar]</strong>baz' ] },

        { 'id':         'B_TEXT-1_SIR',
          'desc':       'Bold reversed selection',
          'pad':        'foo]bar[baz',
          'expected':   [ 'foo<b>[bar]</b>baz',
                          'foo<strong>[bar]</strong>baz' ] },

        { 'id':         'B_I-1_SL',
          'desc':       'Bold selection, partially including italic',
          'pad':        'foo[bar<i>baz]qoz</i>quz',
          'expected':   [ 'foo<b>[bar</b><i><b>baz]</b>qoz</i>quz',
                          'foo<b>[bar<i>baz]</i></b><i>qoz</i>quz',
                          'foo<strong>[bar</strong><i><strong>baz]</strong>qoz</i>quz',
                          'foo<strong>[bar<i>baz]</i></strong><i>qoz</i>quz' ] }
      ]
    },

    { 'desc':       '[HTML5] italic',
      'command':    'italic',
      'tests':      [
        { 'id':         'I_TEXT-1_SI',
          'rte1-id':    'a-italic-0',
          'desc':       'Italicize selection',
          'pad':        'foo[bar]baz',
          'expected':   [ 'foo<i>[bar]</i>baz',
                          'foo<em>[bar]</em>baz' ] }
      ]
    },

    { 'desc':       '[HTML5] underline',
      'command':    'underline',
      'tests':      [
        { 'id':         'U_TEXT-1_SI',
          'rte1-id':    'a-underline-0',
          'desc':       'Underline selection',
          'pad':        'foo[bar]baz',
          'expected':   'foo<u>[bar]</u>baz' }          
      ]
    },

    { 'desc':       '[HTML5] strikethrough',
      'command':    'strikethrough',
      'tests':      [
        { 'id':         'S_TEXT-1_SI',
          'rte1-id':    'a-strikethrough-0',
          'desc':       'Strike-through selection',
          'pad':        'foo[bar]baz',
          'expected':   [ 'foo<s>[bar]</s>baz',
                          'foo<strike>[bar]</strike>baz',
                          'foo<del>[bar]</del>baz' ] }          
      ]
    },

    { 'desc':       '[HTML5] subscript',
      'command':    'subscript',
      'tests':      [
        { 'id':         'SUB_TEXT-1_SI',
          'rte1-id':    'a-subscript-0',
          'desc':       'Change selection to subscript',
          'pad':        'foo[bar]baz',
          'expected':   'foo<sub>[bar]</sub>baz' }
      ]
    },

    { 'desc':       '[HTML5] superscript',
      'command':    'superscript',
      'tests':      [
        { 'id':         'SUP_TEXT-1_SI',
          'rte1-id':    'a-superscript-0',
          'desc':       'Change selection to superscript',
          'pad':        'foo[bar]baz',
          'expected':   'foo<sup>[bar]</sup>baz' }
      ]
    },

    { 'desc':       '[HTML5] createlink',
      'command':    'createlink',
      'tests':      [
        { 'id':         'CL:url_TEXT-1_SI',
          'rte1-id':    'a-createlink-0',
          'desc':       'create a link around the selection',
          'value':      '#foo',
          'pad':        'foo[bar]baz',
          'expected':   'foo<a href="#foo">[bar]</a>baz' }
      ]
    },

    { 'desc':       '[HTML5] formatBlock',
      'command':    'formatblock',
      'tests':      [
        { 'id':         'FB:H1_TEXT-1_SI',
          'rte1-id':    'a-formatblock-0',
          'desc':       'format the selection into a block: use <h1>',
          'value':      'h1',
          'pad':        'foo[bar]baz',
          'expected':   '<h1>foo[bar]baz</h1>' },

        { 'id':         'FB:P_TEXT-1_SI',
          'desc':       'format the selection into a block: use <p>',
          'value':      'p',
          'pad':        'foo[bar]baz',
          'expected':   '<p>foo[bar]baz</p>' },

        { 'id':         'FB:PRE_TEXT-1_SI',
          'desc':       'format the selection into a block: use <pre>',
          'value':      'pre',
          'pad':        'foo[bar]baz',
          'expected':   '<pre>foo[bar]baz</pre>' },

        { 'id':         'FB:ADDRESS_TEXT-1_SI',
          'desc':       'format the selection into a block: use <address>',
          'value':      'address',
          'pad':        'foo[bar]baz',
          'expected':   '<address>foo[bar]baz</address>' },

        { 'id':         'FB:BQ_TEXT-1_SI',
          'desc':       'format the selection into a block: use <blockquote>',
          'value':      'blockquote',
          'pad':        'foo[bar]baz',
          'expected':   '<blockquote>foo[bar]baz</blockquote>' },

        { 'id':         'FB:BQ_BR.BR-1_SM',
          'desc':       'format a multi-line selection into a block: use <blockquote>',
          'command':    'formatblock',
          'value':      'blockquote',
          'pad':        'fo[o<br>bar<br>b]az',
          'expected':   '<blockquote>fo[o<br>bar<br>b]az</blockquote>' }
      ]
    },


    { 'desc':       '[MIDAS] backcolor',
      'command':    'backcolor',
      'tests':      [
        { 'id':         'BC:blue_TEXT-1_SI',
          'rte1-id':    'a-backcolor-0',
          'desc':       'Change background color (note: no non-CSS variant available)',
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
          'rte1-id':    'a-forecolor-0',
          'desc':       'Change the text color',
          'value':      'blue',
          'pad':        'foo[bar]baz',
          'expected':   'foo<font color="blue">[bar]</font>baz' }
      ]
    },

    { 'desc':       '[MIDAS] hilitecolor',
      'command':    'hilitecolor',
      'tests':      [
        { 'id':         'HC:blue_TEXT-1_SI',
          'rte1-id':    'a-hilitecolor-0',
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
          'rte1-id':    'a-fontname-0',
          'desc':       'Change the font name',
          'value':      'arial',
          'pad':        'foo[bar]baz',
          'expected':   'foo<font face="arial">[bar]</font>baz' }
      ]
    },

    { 'desc':       '[MIDAS] fontsize',
      'command':    'fontsize',
      'tests':      [
        { 'id':         'FS:2_TEXT-1_SI',
          'rte1-id':    'a-fontsize-0',
          'desc':       'Change the font size to "2"',
          'value':      '2',
          'pad':        'foo[bar]baz',
          'expected':   'foo<font size="2">[bar]</font>baz' },

        { 'id':         'FS:18px_TEXT-1_SI',
          'desc':       'Change the font size to "18px"',
          'value':      '18px',
          'pad':        'foo[bar]baz',
          'expected':   'foo<font size="18px">[bar]</font>baz' },

        { 'id':         'FS:large_TEXT-1_SI',
          'desc':       'Change the font size to "large"',
          'value':      'large',
          'pad':        'foo[bar]baz',
          'expected':   'foo<font size="large">[bar]</font>baz' }
      ]
    },

    { 'desc':       '[MIDAS] increasefontsize',
      'command':    'increasefontsize',
      'tests':      [
        { 'id':         'INCFS:2_TEXT-1_SI',
          'desc':       'Decrease the font size (to small)',
          'pad':        'foo[bar]baz',
          'expected':   [ 'foo<font size="4">[bar]</font>baz',
                          'foo<font size="+1">[bar]</font>baz',
                          'foo<big>[bar]</big>baz' ] }
      ]
    },

    { 'desc':       '[MIDAS] decreasefontsize',
      'command':    'decreasefontsize',
      'tests':      [
        { 'id':         'DECFS:2_TEXT-1_SI',
          'rte1-id':    'a-decreasefontsize-0',
          'desc':       'Decrease the font size (to small)',
          'pad':        'foo[bar]baz',
          'expected':   [ 'foo<font size="2">[bar]</font>baz',
                          'foo<font size="-1">[bar]</font>baz',
                          'foo<small>[bar]</small>baz' ] }
      ]
    },

    { 'desc':       '[MIDAS] indent (note: accept the de-facto standard indent of 40px)',
      'command':    'indent',
      'tests':      [
        { 'id':         'IND_TEXT-1_SI',
          'rte1-id':    'a-indent-0',
          'desc':       'Indent the text (accept the de-facto standard of 40px indent)',
          'pad':        'foo[bar]baz',
          'checkAttrs': False,
          'expected':   [ '<blockquote>foo[bar]baz</blockquote>',
                          '<div style="margin-left: 40px">foo[bar]baz</div>' ],
          'div': {
            'accOuter': '<div contenteditable="true" style="margin-left: 40px">foo[bar]baz</div>' } }
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
          'rte1-id':    'a-justifycenter-0',
          'desc':       'justify the text centrally',
          'pad':        'foo^bar',
          'expected':   [ '<center>foo^bar</center>',
                          '<p align="center">foo^bar</p>',
                          '<p align="middle">foo^bar</p>',
                          '<div align="center">foo^bar</div>',
                          '<div align="middle">foo^bar</div>' ],
          'div': {
            'accOuter': [ '<div align="center" contenteditable="true">foo^bar</div>',
                          '<div align="middle" contenteditable="true">foo^bar</div>' ] } }
      ]
    },

    { 'desc':       '[MIDAS] justifyfull',
      'command':    'justifyfull',
      'tests':      [
        { 'id':         'JF_TEXT-1_SC',
          'rte1-id':    'a-justifyfull-0',
          'desc':       'justify the text fully',
          'pad':        'foo^bar',
          'expected':   [ '<p align="justify">foo^bar</p>',
                          '<div align="justify">foo^bar</div>' ],
          'div': {
            'accOuter': '<div align="justify" contenteditable="true">foo^bar</div>' } }
      ]
    },

    { 'desc':       '[MIDAS] justifyleft',
      'command':    'justifyleft',
      'tests':      [
        { 'id':         'JL_TEXT-1_SC',
          'rte1-id':    'a-justifyleft-0',
          'desc':       'justify the text left',
          'pad':        'foo^bar',
          'expected':   [ '<p align="left">foo^bar</p>',
                          '<div align="left">foo^bar</div>' ],
          'div': {
            'accOuter': '<div align="left" contenteditable="true">foo^bar</div>' } }
      ]
    },

    { 'desc':       '[MIDAS] justifyright',
      'command':    'justifyright',
      'tests':      [
        { 'id':         'JR_TEXT-1_SC',
          'rte1-id':    'a-justifyright-0',
          'desc':       'justify the text right',
          'pad':        'foo^bar',
          'expected':   [ '<p align="right">foo^bar</p>',
                          '<div align="right">foo^bar</div>' ],
          'div': {
            'accOuter': '<div align="right" contenteditable="true">foo^bar</div>' } }
      ]
    },

    { 'desc':       '[MIDAS] heading',
      'command':    'heading',
      'tests':      [
        { 'id':         'H:H1_TEXT-1_SC',
          'desc':       'create a heading from the paragraph that contains the selection',
          'value':      'h1',
          'pad':        'foo[bar]baz',
          'expected':   '<h1>foo[bar]baz</h1>' }
      ]
    },


    { 'desc':       '[Other] createbookmark',
      'command':    'createbookmark',
      'tests':      [
        { 'id':         'CB:name_TEXT-1_SI',
          'rte1-id':    'a-createbookmark-0',
          'desc':       'create a bookmark (named link) around selection',
          'value':      'created',
          'pad':        'foo[bar]baz',
          'expected':   'foo<a name="created">[bar]</a>baz' }
      ]
    }
  ]
}




