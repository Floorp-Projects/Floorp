
CHANGE_TESTS_CSS = {
  'id':            'CC',
  'caption':       'Change Existing Format to Different Format Tests, using styleWithCSS',
  'checkAttrs':    True,
  'checkStyle':    True,
  'styleWithCSS':  True,

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
          'expected':   'foo<span style="font-style: italic">[barbaz]</span>qoz' },

        { 'id':         'I_B-1_SL',
          'desc':       'Italicize partially bolded text',
          'pad':        'foo[bar<b>baz]</b>qoz',
          'expected':   'foo<span style="font-style: italic">[bar<b>baz]</b></span>qoz',
          'accept':     'foo<span style="font-style: italic">[bar<b>baz</b>}</span>qoz' },

        { 'id':         'I_B-1_SW',
          'desc':       'Italicize bold text, ideally combining both',
          'pad':        'foobar<b>[baz]</b>qoz',
          'expected':   'foobar<span style="font-style: italic; font-weight: bold">[baz]</span>qoz',
          'accept':     'foobar<b><span style="font-style: italic">[baz]</span></b>qoz' }
      ]
    },

    { 'desc':       '[MIDAS] backcolor',
      'command':    'backcolor',
      'tests':      [
        { 'id':         'BC:gray_SPANs:bc:b-1_SW',
          'desc':       'Change background color from blue to gray',
          'value':      'gray',
          'pad':        '<span style="background-color: blue">[foobarbaz]</span>',
          'expected':   '<span style="background-color: gray">[foobarbaz]</span>' },

        { 'id':         'BC:gray_SPANs:bc:b-1_SO',
          'desc':       'Change background color from blue to gray',
          'value':      'gray',
          'pad':        '{<span style="background-color: blue">foobarbaz</span>}',
          'expected':   [ '{<span style="background-color: gray">foobarbaz</span>}',
                          '<span style="background-color: gray">[foobarbaz]</span>' ] },

        { 'id':         'BC:gray_SPANs:bc:b-1_SI',
          'desc':       'Change background color from blue to gray',
          'value':      'gray',
          'pad':        '<span style="background-color: blue">foo[bar]baz</span>',
          'expected':   '<span style="background-color: blue">foo</span><span style="background-color: gray">[bar]</span><span style="background-color: blue">baz</span>',
          'accept':     '<span style="background-color: blue">foo<span style="background-color: gray">[bar]</span>baz</span>' },

        { 'id':         'BC:gray_P-SPANs:bc:b-1_SW',
          'desc':       'Change background color within a paragraph from blue to gray',
          'value':      'gray',
          'pad':        '<p><span style="background-color: blue">[foobarbaz]</span></p>',
          'expected':   [ '<p><span style="background-color: gray">[foobarbaz]</span></p>',
                          '<p style="background-color: gray">[foobarbaz]</p>' ] },

        { 'id':         'BC:gray_P-SPANs:bc:b-2_SW',
          'desc':       'Change background color within a paragraph from blue to gray',
          'value':      'gray',
          'pad':        '<p>foo<span style="background-color: blue">[bar]</span>baz</p>',
          'expected':   '<p>foo<span style="background-color: gray">[bar]</span>baz</p>' },

        { 'id':         'BC:gray_P-SPANs:bc:b-3_SO',
          'desc':       'Change background color within a paragraph from blue to gray (selection encloses more than previous span)',
          'value':      'gray',
          'pad':        '<p>[foo<span style="background-color: blue">barbaz</span>qoz]quz</p>',
          'expected':   '<p><span style="background-color: gray">[foobarbazqoz]</span>quz</p>' },

        { 'id':         'BC:gray_P-SPANs:bc:b-3_SL',
          'desc':       'Change background color within a paragraph from blue to gray (previous span partially selected)',
          'value':      'gray',
          'pad':        '<p>[foo<span style="background-color: blue">bar]baz</span>qozquz</p>',
          'expected':   '<p><span style="background-color: gray">[foobar]</span><span style="background-color: blue">baz</span>qozquz</p>' },

        { 'id':         'BC:gray_SPANs:bc:b-2_SL',
          'desc':       'Change background color from blue to gray on partially covered span, selection extends left',
          'value':      'gray',
          'pad':        'foo [bar <span style="background-color: blue">baz] qoz</span> quz sic',
          'expected':   'foo <span style="background-color: gray">[bar baz]</span><span style="background-color: blue"> qoz</span> quz sic' },

        { 'id':         'BC:gray_SPANs:bc:b-2_SR',
          'desc':       'Change background color from blue to gray on partially covered span, selection extends right',
          'value':      'gray',
          'pad':        'foo bar <span style="background-color: blue">baz [qoz</span> quz] sic',
          'expected':   'foo bar <span style="background-color: blue">baz </span><span style="background-color: gray">[qoz quz]</span> sic' }
      ]
    },

    { 'desc':       '[MIDAS] fontname',
      'command':    'fontname',
      'tests':      [
        { 'id':         'FN:c_SPANs:ff:a-1_SW',
          'desc':       'Change existing font name to new font name, using CSS styling',
          'value':      'courier',
          'pad':        '<span style="font-family: arial">[foobarbaz]</span>',
          'expected':   '<span style="font-family: courier">[foobarbaz]</span>' },

        { 'id':         'FN:c_FONTf:a-1_SW',
          'desc':       'Change existing font name to new font name, using CSS styling',
          'value':      'courier',
          'pad':        '<font face="arial">[foobarbaz]</font>',
          'expected':   [ '<font style="font-family: courier">[foobarbaz]</font>',
                          '<span style="font-family: courier">[foobarbaz]</span>' ] },

        { 'id':         'FN:c_FONTf:a-1_SI',
          'desc':       'Change existing font name to new font name, using CSS styling',
          'value':      'courier',
          'pad':        '<font face="arial">foo[bar]baz</font>',
          'expected':   '<font face="arial">foo</font><span style="font-family: courier">[bar]</span><font face="arial">baz</font>' },

        { 'id':         'FN:a_FONTf:a-1_SI',
          'desc':       'Change existing font name to same font name, using CSS styling (should be noop)',
          'value':      'arial',
          'pad':        '<font face="arial">foo[bar]baz</font>',
          'expected':   '<font face="arial">foo[bar]baz</font>' },

        { 'id':         'FN:a_FONTf:a-1_SW',
          'desc':       'Change existing font name to same font name, using CSS styling (should be noop or perhaps change tag)',
          'value':      'arial',
          'pad':        '<font face="arial">[foobarbaz]</font>',
          'expected':   [ '<font face="arial">[foobarbaz]</font>',
                          '<span style="font-family: arial">[foobarbaz]</span>' ] },

        { 'id':         'FN:a_FONTf:a-1_SO',
          'desc':       'Change existing font name to same font name, using CSS styling (should be noop or perhaps change tag)',
          'value':      'arial',
          'pad':        '{<font face="arial">foobarbaz</font>}',
          'expected':   [ '{<font face="arial">foobarbaz</font>}',
                          '<font face="arial">[foobarbaz]</font>',
                          '{<span style="font-family: arial">foobarbaz</span>}',
                          '<span style="font-family: arial">[foobarbaz]</span>' ] },

        { 'id':         'FN:a_SPANs:ff:a-1_SI',
          'desc':       'Change existing font name to same font name, using CSS styling (should be noop)',
          'value':      'arial',
          'pad':        '<span style="font-family: arial">[foobarbaz]</span>',
          'expected':   '<span style="font-family: arial">[foobarbaz]</span>' },

        { 'id':         'FN:c_FONTf:a-2_SL',
          'desc':       'Change existing font name to new font name, using CSS styling',
          'value':      'courier',
          'pad':        'foo[bar<font face="arial">baz]qoz</font>',
          'expected':   'foo<span style="font-family: courier">[barbaz]</span><font face="arial">qoz</font>' }
      ]
    },

    { 'desc':       '[MIDAS] fontsize',
      'command':    'fontsize',
      'tests':      [
        { 'id':         'FS:1_SPANs:fs:l-1_SW',
          'desc':       'Change existing font size to new size, using CSS styling',
          'value':      '1',
          'pad':        '<span style="font-size: large">[foobarbaz]</span>',
          'expected':   '<span style="font-size: x-small">[foobarbaz]</span>' },

        { 'id':         'FS:large_SPANs:fs:l-1_SW',
          'desc':       'Change existing font size to same size (should be noop)',
          'value':      'large',
          'pad':        '<span style="font-size: large">[foobarbaz]</span>',
          'expected':   '<span style="font-size: large">[foobarbaz]</span>' },

        { 'id':         'FS:18px_SPANs:fs:l-1_SW',
          'desc':       'Change existing font size to equivalent px size (should be noop, or change unit)',
          'value':      '18px',
          'pad':        '<span style="font-size: large">[foobarbaz]</span>',
          'expected':   [ '<span style="font-size: 18px">[foobarbaz]</span>',
                          '<span style="font-size: large">[foobarbaz]</span>' ] },

        { 'id':         'FS:4_SPANs:fs:l-1_SW',
          'desc':       'Change existing font size to equivalent numeric size (should be noop)',
          'value':      '4',
          'pad':        '<span style="font-size: large">[foobarbaz]</span>',
          'expected':   '<span style="font-size: large">[foobarbaz]</span>' },

        { 'id':         'FS:4_SPANs:fs:18px-1_SW',
          'desc':       'Change existing font size to equivalent numeric size (should be noop)',
          'value':      '4',
          'pad':        '<span style="font-size: 18px">[foobarbaz]</span>',
          'expected':   '<span style="font-size: 18px">[foobarbaz]</span>' },
          
        { 'id':         'FS:larger_SPANs:fs:l-1_SI',
          'desc':       'Change selection to use next larger font',
          'value':      'larger',
          'pad':        '<span style="font-size: large">foo[bar]baz</span>',
          'expected':   [ '<span style="font-size: large">foo<span style="font-size: x-large">[bar]</span>baz</span>',
                          '<span style="font-size: large">foo</span><span style="font-size: x-large">[bar]</span><span style="font-size: large">baz</span>' ],
          'accept':     '<span style="font-size: large">foo<font size="larger">[bar]</font>baz</span>' },
                        
        { 'id':         'FS:smaller_SPANs:fs:l-1_SI',
          'desc':       'Change selection to use next smaller font',
          'value':      'smaller',
          'pad':        '<span style="font-size: large">foo[bar]baz</span>',
          'expected':   [ '<span style="font-size: large">foo<span style="font-size: medium">[bar]</span>baz</span>',
                          '<span style="font-size: large">foo</span><span style="font-size: medium">[bar]</span><span style="font-size: large">baz</span>' ],
          'accept':     '<span style="font-size: large">foo<font size="smaller">[bar]</font>baz</span>' }
      ]
    }
  ]
}
