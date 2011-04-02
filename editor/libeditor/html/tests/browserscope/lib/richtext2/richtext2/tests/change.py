
CHANGE_TESTS = {
  'id':            'C',
  'caption':       'Change Existing Format to Different Format Tests',
  'checkAttrs':    True,
  'checkStyle':    True,
  'styleWithCSS':  False,

  'Proposed': [
    { 'desc':       '',
      'command':    '',
      'tests':      [
      ]
    },

    { 'desc':       '[HTML5] italic',
      'command':    'italic',
      'tests':      [
        { 'id':         'I_I-1_SL',
          'desc':       'Italicize partially italicized text',
          'pad':        'foo[bar<i>baz]</i>qoz',
          'expected':   'foo<i>[barbaz]</i>qoz' },

        { 'id':         'I_B-I-1_SO',
          'desc':       'Italicize partially italicized text in bold context',
          'pad':        '<b>foo[bar<i>baz</i>}</b>',
          'expected':   '<b>foo<i>[barbaz]</i></b>' }
      ]
    },

    { 'desc':       '[HTML5] underline',
      'command':    'underline',
      'tests':      [
        { 'id':         'U_U-1_SO',
          'desc':       'Underline partially underlined text',
          'pad':        'foo[bar<u>baz</u>qoz]quz', 
          'expected':   'foo<u>[barbazqoz]</u>quz' },

        { 'id':         'U_U-1_SL',
          'desc':       'Underline partially underlined text',
          'pad':        'foo[bar<u>baz]qoz</u>quz', 
          'expected':   'foo<u>[barbaz]qoz</u>quz' },

        { 'id':         'U_S-U-1_SO',
          'desc':       'Underline partially underlined text in striked context',
          'pad':        '<s>foo[bar<u>baz</u>}</s>', 
          'expected':   '<s>foo<u>[barbaz]</u></s>' }
      ]
    },


    { 'desc':       '[MIDAS] backcolor',
      'command':    'backcolor',
      'tests':      [
        { 'id':         'BC:842_FONTs:bc:fca-1_SW',
          'rte1-id':    'c-backcolor-0',
          'desc':       'Change background color to new color',
          'value':      '#884422',
          'pad':        '<font style="background-color: #ffccaa">[foobarbaz]</font>',
          'expected':   [ '<font style="background-color: #884422">[foobarbaz]</font>',
                          '<span style="background-color: #884422">[foobarbaz]</span>' ] },

        { 'id':         'BC:00f_SPANs:bc:f00-1_SW',
          'rte1-id':    'c-backcolor-2',
          'desc':       'Change background color to new color',
          'value':      '#0000ff',
          'pad':        '<span style="background-color: #ff0000">[foobarbaz]</span>',
          'expected':   [ '<font style="background-color: #0000ff">[foobarbaz]</font>',
                          '<span style="background-color: #0000ff">[foobarbaz]</span>' ] },

        { 'id':         'BC:ace_FONT.ass.s:bc:rgb-1_SW',
          'rte1-id':    'c-backcolor-1',
          'desc':       'Change background color in styled span to new color',
          'value':      '#aaccee',
          'pad':        '<span class="Apple-style-span" style="background-color: rgb(255, 0, 0)">[foobarbaz]</span>',
          'expected':   [ '<font style="background-color: #aaccee">[foobarbaz]</font>',
                          '<span style="background-color: #aaccee">[foobarbaz]</span>' ] }
      ]
    },

    { 'desc':       '[MIDAS] forecolor',
      'command':    'forecolor',
      'tests':      [
        { 'id':         'FC:g_FONTc:b-1_SW',
          'rte1-id':    'c-forecolor-0',
          'desc':       'Change the text color (without CSS)',
          'value':      'green',
          'pad':        '<font color="blue">[foobarbaz]</font>',
          'expected':   '<font color="green">[foobarbaz]</font>' },

        { 'id':         'FC:g_SPANs:c:g-1_SW',
          'rte1-id':    'c-forecolor-1',
          'desc':       'Change the text color from a styled span (without CSS)',
          'value':      'green',
          'pad':        '<span style="color: blue">[foobarbaz]</span>',
          'expected':   '<font color="green">[foobarbaz]</font>' },

        { 'id':         'FC:g_FONTc:b.s:c:r-1_SW',
          'rte1-id':    'c-forecolor-2',
          'desc':       'Change the text color from conflicting color and style (without CSS)',
          'value':      'green',
          'pad':        '<font color="blue" style="color: red">[foobarbaz]</font>',
          'expected':   '<font color="green">[foobarbaz]</font>' },

        { 'id':         'FC:g_FONTc:b.sz:6-1_SI',
          'desc':       'Change the font color in content with a different font size and font color',
          'value':      'green',
          'pad':        '<font color="blue" size="6">foo[bar]baz</font>',
          'expected':   [ '<font color="blue" size="6">foo<font color="green">[bar]</font>baz</font>',
                          '<font size="6"><font color="blue">foo<font color="green">[bar]</font><font color="blue">baz</font></font>' ] }
      ]
    },

    { 'desc':       '[MIDAS] hilitecolor',
      'command':    'hilitecolor',
      'tests':      [
        { 'id':         'HC:g_FONTs:c:b-1_SW',
          'rte1-id':    'c-hilitecolor-0',
          'desc':       'Change the hilite color (without CSS)',
          'value':      'green',
          'pad':        '<font style="background-color: blue">[foobarbaz]</font>',
          'expected':   [ '<font style="background-color: green">[foobarbaz]</font>',
                          '<span style="background-color: green">[foobarbaz]</span>' ] },

        { 'id':         'HC:g_SPANs:c:g-1_SW',
          'rte1-id':    'c-hilitecolor-2',
          'desc':       'Change the hilite color from a styled span (without CSS)',
          'value':      'green',
          'pad':        '<span style="background-color: blue">[foobarbaz]</span>',
          'expected':   '<span style="background-color: green">[foobarbaz]</span>' },

        { 'id':         'HC:g_SPAN.ass.s:c:rgb-1_SW',
          'rte1-id':    'c-hilitecolor-1',
          'desc':       'Change the hilite color from a styled span (without CSS)',
          'value':      'green',
          'pad':        '<span class="Apple-style-span" style="background-color: rgb(255, 0, 0);">[foobarbaz]</span>',
          'expected':   '<span style="background-color: green">[foobarbaz]</span>' }
      ]
    },

    { 'desc':       '[MIDAS] fontname',
      'command':    'fontname',
      'tests':      [
        { 'id':         'FN:c_FONTf:a-1_SW',
          'rte1-id':    'c-fontname-0',
          'desc':       'Change existing font name to new font name (without CSS)',
          'value':      'courier',
          'pad':        '<font face="arial">[foobarbaz]</font>',
          'expected':   '<font face="courier">[foobarbaz]</font>' },

        { 'id':         'FN:c_SPANs:ff:a-1_SW',
          'rte1-id':    'c-fontname-1',
          'desc':       'Change existing font name from style to new font name (without CSS)',
          'value':      'courier',
          'pad':        '<span style="font-family: arial">[foobarbaz]</span>',
          'expected':   '<font face="courier">[foobarbaz]</font>' },

        { 'id':         'FN:c_FONTf:a.s:ff:v-1_SW',
          'rte1-id':    'c-fontname-2',
          'desc':       'Change existing font name with conflicting face and style to new font name (without CSS)',
          'value':      'courier',
          'pad':        '<font face="arial" style="font-family: verdana">[foobarbaz]</font>',
          'expected':   '<font face="courier">[foobarbaz]</font>' },

        { 'id':          'FN:c_FONTf:a-1_SI',
          'desc':        'Change existing font name to new font name, text partially selected',
          'value':       'courier',
          'pad':         '<font face="arial">foo[bar]baz</font>',
          'expected':    '<font face="arial">foo</font><font face="courier">[bar]</font><font face="arial">baz</font>',
          'accept':      '<font face="arial">foo<font face="courier">[bar]</font>baz</font>' },

        { 'id':          'FN:c_FONTf:a-2_SL',
          'desc':        'Change existing font name to new font name, using CSS styling',
          'value':       'courier',
          'pad':         'foo[bar<font face="arial">baz]qoz</font>',
          'expected':    'foo<font face="courier">[barbaz]</font><font face="arial">qoz</font>' },

        { 'id':         'FN:c_FONTf:v-FONTf:a-1_SW',
          'rte1-id':    'c-fontname-3',
          'desc':       'Change existing font name in nested <font> tags to new font name (without CSS)',
          'value':      'courier',
          'pad':        '<font face="verdana"><font face="arial">[foobarbaz]</font></font>',
          'expected':   '<font face="courier">[foobarbaz]</font>',
          'accept':     '<font face="verdana"><font face="courier">[foobarbaz]</font></font>' },

        { 'id':         'FN:c_SPANs:ff:v-FONTf:a-1_SW',
          'rte1-id':    'c-fontname-4',
          'desc':       'Change existing font name in nested mixed tags to new font name (without CSS)',
          'value':      'courier',
          'pad':        '<span style="font-family: verdana"><font face="arial">[foobarbaz]</font></span>',
          'expected':   '<font face="courier">[foobarbaz]</font>',
          'accept':     '<span style="font-family: verdana"><font face="courier">[foobarbaz]</font></span>' }
      ]
    },

    { 'desc':       '[MIDAS] fontsize',
      'command':    'fontsize',
      'tests':      [
        { 'id':         'FS:1_FONTsz:4-1_SW',
          'rte1-id':    'c-fontsize-0',
          'desc':       'Change existing font size to new size (without CSS)',
          'value':      '1',
          'pad':        '<font size="4">[foobarbaz]</font>',
          'expected':   '<font size="1">[foobarbaz]</font>' },
                        
        { 'id':         'FS:1_SPAN.ass.s:fs:large-1_SW',
          'rte1-id':    'c-fontsize-1',
          'desc':       'Change existing font size from styled span to new size (without CSS)',
          'value':      '1',
          'pad':        '<span class="Apple-style-span" style="font-size: large">[foobarbaz]</span>',
          'expected':   '<font size="1">[foobarbaz]</font>' },
                        
        { 'id':         'FS:5_FONTsz:1.s:fs:xs-1_SW',
          'rte1-id':    'c-fontsize-2',
          'desc':       'Change existing font size from tag with conflicting size and style to new size (without CSS)',
          'value':      '5',
          'pad':        '<font size="1" style="font-size:x-small">[foobarbaz]</font>',
          'expected':   '<font size="5">[foobarbaz]</font>' },
                        
        { 'id':         'FS:2_FONTc:b.sz:6-1_SI',
          'desc':       'Change the font size in content with a different font size and font color',
          'value':      '2',
          'pad':        '<font color="blue" size="6">foo[bar]baz</font>',
          'expected':   [ '<font color="blue" size="6">foo<font size="2">[bar]</font>baz</font>',
                          '<font color="blue"><font size="6">foo</font><font size="2">[bar]</font><font size="6">baz</font></font>' ] },
          
        { 'id':         'FS:larger_FONTsz:4',
          'desc':       'Change selection to use next larger font',
          'value':      'larger',
          'pad':        '<font size="4">foo[bar]baz</font>',
          'expected':   '<font size="4">foo<font size="larger">[bar]</font>baz</font>',
          'accept':     '<font size="4">foo</font><font size="5">[bar]</font><font size="4">baz</font>' },
                        
        { 'id':         'FS:smaller_FONTsz:4',
          'desc':       'Change selection to use next smaller font',
          'value':      'smaller',
          'pad':        '<font size="4">foo[bar]baz</font>',
          'expected':   '<font size="4">foo<font size="smaller">[bar]</font>baz</font>',
          'accept':     '<font size="4">foo</font><font size="3">[bar]</font><font size="4">baz</font>' }
      ]
    },

    { 'desc':       '[MIDAS] formatblock',
      'command':    'formatblock',
      'tests':      [
        { 'id':         'FB:h1_ADDRESS-1_SW',
          'desc':       'change block from <address> to <h1>',
          'value':      'h1',
          'pad':        '<address>foo [bar] baz</address>',
          'expected':   '<h1>foo [bar] baz</h1>' },

        { 'id':         'FB:h1_ADDRESS-FONTsz:4-1_SO',
          'desc':       'change block from <address> with partially formatted content to <h1>',
          'value':      'h1',
          'pad':        '<address>foo [<font size="4">bar</font>] baz</address>',
          'expected':   '<h1>foo [bar] baz</h1>' },

        { 'id':         'FB:h1_ADDRESS-FONTsz:4-1_SW',
          'desc':       'change block from <address> with partially formatted content to <h1>',
          'value':      'h1',
          'pad':        '<address>foo <font size="4">[bar]</font> baz</address>',
          'expected':   '<h1>foo [bar] baz</h1>' },

        { 'id':         'FB:h1_ADDRESS-FONT.ass.sz:4-1_SW',
          'desc':       'change block from <address> with partially formatted content to <h1>',
          'value':      'h1',
          'pad':        '<address>foo <font class="Apple-style-span" size="4">[bar]</font> baz</address>',
          'expected':   '<h1>foo [bar] baz</h1>' }
      ]
    }
  ]
}

