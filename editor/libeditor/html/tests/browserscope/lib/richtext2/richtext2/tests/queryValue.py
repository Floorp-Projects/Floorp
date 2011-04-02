
QUERYVALUE_TESTS = {
  'id':            'QV',
  'caption':       'queryCommandValue Tests',
  'checkAttrs':    False,
  'checkStyle':    False,
  'styleWithCSS':  False,

  'Proposed': [
    { 'desc':       '',
      'tests':      [
      ]
    },

    { 'desc':       '[HTML5] query bold value',
      'qcvalue':    'bold',
      'tests':      [
        { 'id':         'B_TEXT_SI',
          'desc':       'query the "bold" value',
          'pad':        'foo[bar]baz',
          'expected':   'false' },

        { 'id':         'B_B-1_SI',
          'desc':       'query the "bold" value',
          'pad':        '<b>foo[bar]baz</b>',
          'expected':   'true' },

        { 'id':         'B_STRONG-1_SI',
          'desc':       'query the "bold" value',
          'pad':        '<strong>foo[bar]baz</strong>',
          'expected':   'true' },

        { 'id':         'B_SPANs:fw:b-1_SI',
          'desc':       'query the "bold" value',
          'pad':        '<span style="font-weight: bold">foo[bar]baz</span>',
          'expected':   'true' },

        { 'id':         'B_SPANs:fw:n-1_SI',
          'desc':       'query the "bold" value',
          'pad':        '<span style="font-weight: normal">foo[bar]baz</span>',
          'expected':   'false' },

        { 'id':         'B_Bs:fw:n-1_SI',
          'desc':       'query the "bold" value',
          'pad':        '<b><span style="font-weight: normal">foo[bar]baz</span></b>',
          'expected':   'false' },

        { 'id':         'B_SPAN.b-1_SI',
          'desc':       'query the "bold" value',
          'pad':        '<span class="b">foo[bar]baz</span>',
          'expected':   'true' },

        { 'id':         'B_MYB-1-SI',
          'desc':       'query the "bold" value',
          'pad':        '<myb>foo[bar]baz</myb>',
          'expected':   'true' }
      ]
    },

    { 'desc':       '[HTML5] query italic value',
      'qcvalue':    'italic',
      'tests':      [
        { 'id':         'I_TEXT_SI',
          'desc':       'query the "bold" value',
          'pad':        'foo[bar]baz',
          'expected':   'false' },

        { 'id':         'I_I-1_SI',
          'desc':       'query the "bold" value',
          'pad':        '<i>foo[bar]baz</i>',
          'expected':   'true' },

        { 'id':         'I_EM-1_SI',
          'desc':       'query the "bold" value',
          'pad':        '<em>foo[bar]baz</em>',
          'expected':   'true' },

        { 'id':         'I_SPANs:fs:i-1_SI',
          'desc':       'query the "bold" value',
          'pad':        '<span style="font-style: italic">foo[bar]baz</span>',
          'expected':   'true' },

        { 'id':         'I_SPANs:fs:n-1_SI',
          'desc':       'query the "bold" value',
          'pad':        '<span style="font-style: normal">foo[bar]baz</span>',
          'expected':   'false' },

        { 'id':         'I_I-SPANs:fs:n-1_SI',
          'desc':       'query the "bold" value',
          'pad':        '<i><span style="font-style: normal">foo[bar]baz</span></i>',
          'expected':   'false' },

        { 'id':         'I_SPAN.i-1_SI',
          'desc':       'query the "italic" value',
          'pad':        '<span class="i">foo[bar]baz</span>',
          'expected':   'true' },

        { 'id':         'I_MYI-1-SI',
          'desc':       'query the "italic" value',
          'pad':        '<myi>foo[bar]baz</myi>',
          'expected':   'true' }
      ]
    },

    { 'desc':       '[HTML5] query block formatting value',
      'qcvalue':    'formatblock',
      'tests':      [
        { 'id':         'FB_TEXT-1_SC',
          'desc':       'query the "formatBlock" value',
          'pad':        'foobar^baz',
          'expected':   '',
          'accept':     'normal' },

        { 'id':         'FB_H1-1_SC',
          'desc':       'query the "formatBlock" value',
          'pad':        '<h1>foobar^baz</h1>',
          'expected':   'h1' },

        { 'id':         'FB_PRE-1_SC',
          'desc':       'query the "formatBlock" value',
          'pad':        '<pre>foobar^baz</pre>',
          'expected':   'pre' },

        { 'id':         'FB_BQ-1_SC',
          'desc':       'query the "formatBlock" value',
          'pad':        '<blockquote>foobar^baz</blockquote>',
          'expected':   'blockquote' },

        { 'id':         'FB_ADDRESS-1_SC',
          'desc':       'query the "formatBlock" value',
          'pad':        '<address>foobar^baz</address>',
          'expected':   'address' },

        { 'id':         'FB_H1-H2-1_SC',
          'desc':       'query the "formatBlock" value',
          'pad':        '<h1>foo<h2>ba^r</h2>baz</h1>',
          'expected':   'h2' },

        { 'id':         'FB_H1-H2-1_SL',
          'desc':       'query the "formatBlock" value on oblique selection (outermost formatting expected)',
          'pad':        '<h1>fo[o<h2>ba]r</h2>baz</h1>',
          'expected':   'h1' },

        { 'id':         'FB_H1-H2-1_SR',
          'desc':       'query the "formatBlock" value on oblique selection (outermost formatting expected)',
          'pad':        '<h1>foo<h2>b[ar</h2>ba]z</h1>',
          'expected':   'h1' },

        { 'id':         'FB_TEXT-ADDRESS-1_SL',
          'desc':       'query the "formatBlock" value on oblique selection (outermost formatting expected)',
          'pad':        'fo[o<ADDRESS>ba]r</ADDRESS>baz',
          'expected':   '',
          'accept':     'normal' },

        { 'id':         'FB_TEXT-ADDRESS-1_SR',
          'desc':       'query the "formatBlock" value on oblique selection (outermost formatting expected)',
          'pad':        'foo<ADDRESS>b[ar</ADDRESS>ba]z',
          'expected':   '',
          'accept':     'normal' },

        { 'id':         'FB_H1-H2.TEXT.H2-1_SM',
          'desc':       'query the "formatBlock" value on oblique selection (outermost formatting expected)',
          'pad':        '<h1><h2>fo[o</h2>bar<h2>b]az</h2></h1>',
          'expected':   'h1' }
      ]
    },

    { 'desc':       '[MIDAS] query heading type',
      'qcvalue':    'heading',
      'tests':      [
        { 'id':         'H_H1-1_SC',
          'desc':       'query the "heading" type',
          'pad':        '<h1>foobar^baz</h1>',
          'expected':   'h1',
          'accept':     '<h1>' },

        { 'id':         'H_H3-1_SC',
          'desc':       'query the "heading" type',
          'pad':        '<h3>foobar^baz</h3>',
          'expected':   'h3',
          'accept':     '<h3>' },

        { 'id':         'H_H1-H2-H3-H4-1_SC',
          'desc':       'query the "heading" type within nested heading tags',
          'pad':        '<h1><h2><h3><h4>foobar^baz</h4></h3></h2></h1>',
          'expected':   'h4',
          'accept':     '<h4>' },

        { 'id':         'H_P-1_SC',
          'desc':       'query the "heading" type outside of a heading',
          'pad':        '<p>foobar^baz</p>',
          'expected':   '' }
      ]
    },

    { 'desc':       '[MIDAS] query font name',
      'qcvalue':    'fontname',
      'tests':      [
        { 'id':         'FN_FONTf:a-1_SI',
          'rte1-id':    'q-fontname-0',
          'desc':       'query the "fontname" value',
          'pad':        '<font face="arial">foo[bar]baz</font>',
          'expected':   'arial' },

        { 'id':         'FN_SPANs:ff:a-1_SI',
          'rte1-id':    'q-fontname-1',
          'desc':       'query the "fontname" value',
          'pad':        '<span style="font-family: arial">foo[bar]baz</span>',
          'expected':   'arial' },

        { 'id':         'FN_FONTf:a.s:ff:c-1_SI',
          'rte1-id':    'q-fontname-2',
          'desc':       'query the "fontname" value',
          'pad':        '<font face="arial" style="font-family: courier">foo[bar]baz</font>',
          'expected':   'courier' },

        { 'id':         'FN_FONTf:a-FONTf:c-1_SI',
          'rte1-id':    'q-fontname-3',
          'desc':       'query the "fontname" value',
          'pad':        '<font face="arial"><font face="courier">foo[bar]baz</font></font>',
          'expected':   'courier' },

        { 'id':         'FN_SPANs:ff:c-FONTf:a-1_SI',
          'rte1-id':    'q-fontname-4',
          'desc':       'query the "fontname" value',
          'pad':        '<span style="font-family: courier"><font face="arial">foo[bar]baz</font></span>',
          'expected':   'arial' },

        { 'id':         'FN_SPAN.fs18px-1_SI',
          'desc':       'query the "fontname" value',
          'pad':        '<span class="courier">foo[bar]baz</span>',
          'expected':   'courier' },

        { 'id':         'FN_MYCOURIER-1-SI',
          'desc':       'query the "fontname" value',
          'pad':        '<mycourier>foo[bar]baz</mycourier>',
          'expected':   'courier' }
      ]
    },

    { 'desc':       '[MIDAS] query font size',
      'qcvalue':    'fontsize',
      'tests':      [
        { 'id':         'FS_FONTsz:4-1_SI',
          'rte1-id':    'q-fontsize-0',
          'desc':       'query the "fontsize" value',
          'pad':        '<font size=4>foo[bar]baz</font>',
          'expected':   '18px' },

        { 'id':         'FS_FONTs:fs:l-1_SI',
          'desc':       'query the "fontsize" value',
          'pad':        '<font style="font-size: large">foo[bar]baz</font>',
          'expected':   '18px' },

        { 'id':         'FS_FONT.ass.s:fs:l-1_SI',
          'rte1-id':    'q-fontsize-1',
          'desc':       'query the "fontsize" value',
          'pad':        '<font class="Apple-style-span" style="font-size: large">foo[bar]baz</font>',
          'expected':   '18px' },

        { 'id':         'FS_FONTsz:1.s:fs:xl-1_SI',
          'rte1-id':    'q-fontsize-2',
          'desc':       'query the "fontsize" value',
          'pad':        '<font size=1 style="font-size: x-large">foo[bar]baz</font>',
          'expected':   '24px' },

        { 'id':         'FS_SPAN.large-1_SI',
          'desc':       'query the "fontsize" value',
          'pad':        '<span class="large">foo[bar]baz</span>',
          'expected':   'large' },

        { 'id':         'FS_SPAN.fs18px-1_SI',
          'desc':       'query the "fontsize" value',
          'pad':        '<span class="fs18px">foo[bar]baz</span>',
          'expected':   '18px' },

        { 'id':         'FA_MYLARGE-1-SI',
          'desc':       'query the "fontsize" value',
          'pad':        '<mylarge>foo[bar]baz</mylarge>',
          'expected':   'large' },

        { 'id':         'FA_MYFS18PX-1-SI',
          'desc':       'query the "fontsize" value',
          'pad':        '<myfs18px>foo[bar]baz</myfs18px>',
          'expected':   '18px' }
      ]
    },

    { 'desc':       '[MIDAS] query background color',
      'qcvalue':    'backcolor',
      'tests':      [
        { 'id':         'BC_FONTs:bc:fca-1_SI',
          'rte1-id':    'q-backcolor-0',
          'desc':       'query the "backcolor" value',
          'pad':        '<font style="background-color: #ffccaa">foo[bar]baz</font>',
          'expected':   '#ffccaa' },

        { 'id':         'BC_SPANs:bc:abc-1_SI',
          'rte1-id':    'q-backcolor-2',
          'desc':       'query the "backcolor" value',
          'pad':        '<span style="background-color: #aabbcc">foo[bar]baz</span>',
          'expected':   '#aabbcc' },

        { 'id':         'BC_FONTs:bc:084-SPAN-1_SI',
          'desc':       'query the "backcolor" value, where the color was set on an ancestor',
          'pad':        '<font style="background-color: #008844"><span>foo[bar]baz</span></font>',
          'expected':   '#008844' },

        { 'id':         'BC_SPANs:bc:cde-SPAN-1_SI',
          'desc':       'query the "backcolor" value, where the color was set on an ancestor',
          'pad':        '<span style="background-color: #ccddee"><span>foo[bar]baz</span></span>',
          'expected':   '#ccddee' },

        { 'id':         'BC_SPAN.ass.s:bc:rgb-1_SI',
          'rte1-id':    'q-backcolor-1',
          'desc':       'query the "backcolor" value',
          'pad':        '<span class="Apple-style-span" style="background-color: rgb(255, 0, 0)">foo[bar]baz</span>',
          'expected':   '#ff0000' },

        { 'id':         'BC_SPAN.bcred-1_SI',
          'desc':       'query the "backcolor" value',
          'pad':        '<span class="bcred">foo[bar]baz</span>',
          'expected':   'red' },

        { 'id':         'BC_MYBCRED-1-SI',
          'desc':       'query the "backcolor" value',
          'pad':        '<mybcred>foo[bar]baz</mybcred>',
          'expected':   'red' }
      ]
    },

    { 'desc':       '[MIDAS] query text color',
      'qcvalue':    'forecolor',
      'tests':      [
        { 'id':         'FC_FONTc:f00-1_SI',
          'rte1-id':    'q-forecolor-0',
          'desc':       'query the "forecolor" value',
          'pad':        '<font color="#ff0000">foo[bar]baz</font>',
          'expected':   '#ff0000' },

        { 'id':         'FC_SPANs:c:0f0-1_SI',
          'rte1-id':    'q-forecolor-1',
          'desc':       'query the "forecolor" value',
          'pad':        '<span style="color: #00ff00">foo[bar]baz</span>',
          'expected':   '#00ff00' },

        { 'id':         'FC_FONTc:333.s:c:999-1_SI',
          'rte1-id':    'q-forecolor-2',
          'desc':       'query the "forecolor" value',
          'pad':        '<font color="#333333" style="color: #999999">foo[bar]baz</font>',
          'expected':   '#999999' },

        { 'id':         'FC_FONTc:641-SPAN-1_SI',
          'desc':       'query the "forecolor" value, where the color was set on an ancestor',
          'pad':        '<font color="#664411"><span>foo[bar]baz</span></font>',
          'expected':   '#664411' },

        { 'id':         'FC_SPANs:c:d95-SPAN-1_SI',
          'desc':       'query the "forecolor" value, where the color was set on an ancestor',
          'pad':        '<span style="color: #dd9955"><span>foo[bar]baz</span></span>',
          'expected':   '#dd9955' },

        { 'id':         'FC_SPAN.red-1_SI',
          'desc':       'query the "forecolor" value',
          'pad':        '<span class="red">foo[bar]baz</span>',
          'expected':   'red' },

        { 'id':         'FC_MYRED-1-SI',
          'desc':       'query the "forecolor" value',
          'pad':        '<myred>foo[bar]baz</myred>',
          'expected':   'red' }
      ]
    },

    { 'desc':       '[MIDAS] query hilight color (same as background color)',
      'qcvalue':    'hilitecolor',
      'tests':      [
        { 'id':         'HC_FONTs:bc:fc0-1_SI',
          'rte1-id':    'q-hilitecolor-0',
          'desc':       'query the "hilitecolor" value',
          'pad':        '<font style="background-color: #ffcc00">foo[bar]baz</font>',
          'expected':   '#ffcc00' },

        { 'id':         'HC_SPANs:bc:a0c-1_SI',
          'rte1-id':    'q-hilitecolor-2',
          'desc':       'query the "hilitecolor" value',
          'pad':        '<span style="background-color: #aa00cc">foo[bar]baz</span>',
          'expected':   '#aa00cc' },

        { 'id':         'HC_SPAN.ass.s:bc:rgb-1_SI',
          'rte1-id':    'q-hilitecolor-1',
          'desc':       'query the "hilitecolor" value',
          'pad':        '<span class="Apple-style-span" style="background-color: rgb(255, 0, 0)">foo[bar]baz</span>',
          'expected':   '#ff0000' },

        { 'id':         'HC_FONTs:bc:83e-SPAN-1_SI',
          'desc':       'query the "hilitecolor" value, where the color was set on an ancestor',
          'pad':        '<font style="background-color: #8833ee"><span>foo[bar]baz</span></font>',
          'expected':   '#8833ee' },

        { 'id':         'HC_SPANs:bc:b12-SPAN-1_SI',
          'desc':       'query the "hilitecolor" value, where the color was set on an ancestor',
          'pad':        '<span style="background-color: #bb1122"><span>foo[bar]baz</span></span>',
          'expected':   '#bb1122' },

        { 'id':         'HC_SPAN.bcred-1_SI',
          'desc':       'query the "hilitecolor" value',
          'pad':        '<span class="bcred">foo[bar]baz</span>',
          'expected':   'red' },

        { 'id':         'HC_MYBCRED-1-SI',
          'desc':       'query the "hilitecolor" value',
          'pad':        '<mybcred>foo[bar]baz</mybcred>',
          'expected':   'red' }
      ]
    }
  ]
}

QUERYVALUE_TESTS_CSS = {
  'id':           'QVC',
  'caption':      'queryCommandValue Tests, using styleWithCSS',
  'checkAttrs':   False,
  'checkStyle':   False,
  'styleWithCSS': True,
  
  'Proposed':     QUERYVALUE_TESTS['Proposed']
}

