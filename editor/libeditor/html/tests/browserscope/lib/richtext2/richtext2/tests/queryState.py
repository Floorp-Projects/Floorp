
QUERYSTATE_TESTS = {
  'id':            'QS',
  'caption':       'queryCommandState Tests',
  'checkAttrs':    False,
  'checkStyle':    False,
  'styleWithCSS':  False,

  'Proposed': [
    { 'desc':       '',
      'qcstate':    '',
      'tests':      [
      ]
    },

    { 'desc':       'query bold state',
      'qcstate':    'bold',
      'tests':      [
        { 'id':         'B_TEXT_SI',
          'rte1-id':    'q-bold-0',
          'desc':       'query the "bold" state',
          'pad':        'foo[bar]baz',
          'expected':   False },

        { 'id':         'B_B-1_SI',
          'rte1-id':    'q-bold-1',
          'desc':       'query the "bold" state',
          'pad':        '<b>foo[bar]baz</b>',
          'expected':   True },

        { 'id':         'B_STRONG-1_SI',
          'rte1-id':    'q-bold-2',
          'desc':       'query the "bold" state',
          'pad':        '<strong>foo[bar]baz</strong>',
          'expected':   True },

        { 'id':         'B_SPANs:fw:b-1_SI',
          'rte1-id':    'q-bold-3',
          'desc':       'query the "bold" state',
          'pad':        '<span style="font-weight: bold">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'B_SPANs:fw:n-1_SI',
          'desc':       'query the "bold" state',
          'pad':        '<span style="font-weight: normal">foo[bar]baz</span>',
          'expected':   False },

        { 'id':         'B_Bs:fw:n-1_SI',
          'rte1-id':    'q-bold-4',
          'desc':       'query the "bold" state',
          'pad':        '<span style="font-weight: normal">foo[bar]baz</span>',
          'expected':   False },

        { 'id':         'B_B-SPANs:fw:n-1_SI',
          'rte1-id':    'q-bold-5',
          'desc':       'query the "bold" state',
          'pad':        '<b><span style="font-weight: normal">foo[bar]baz</span></b>',
          'expected':   False },

        { 'id':         'B_SPAN.b-1-SI',
          'desc':       'query the "bold" state',
          'pad':        '<span class="b">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'B_MYB-1-SI',
          'desc':       'query the "bold" state',
          'pad':        '<myb>foo[bar]baz</myb>',
          'expected':   True },

        { 'id':         'B_B-I-1_SC',
          'desc':       'query the "bold" state, bold tag not immediate parent',
          'pad':        '<b>foo<i>ba^r</i>baz</b>',
          'expected':   True },

        { 'id':         'B_B-I-1_SL',
          'desc':       'query the "bold" state, selection partially in child element',
          'pad':        '<b>fo[o<i>b]ar</i>baz</b>',
          'expected':   True },

        { 'id':         'B_B-I-1_SR',
          'desc':       'query the "bold" state, selection partially in child element',
          'pad':        '<b>foo<i>ba[r</i>b]az</b>',
          'expected':   True },

        { 'id':         'B_STRONG-I-1_SC',
          'desc':       'query the "bold" state, bold tag not immediate parent',
          'pad':        '<strong>foo<i>ba^r</i>baz</strong>',
          'expected':   True },

        { 'id':         'B_B-I-U-1_SC',
          'desc':       'query the "bold" state, bold tag not immediate parent',
          'pad':        '<b>foo<i>bar<u>b^az</u></i></strong>',
          'expected':   True },

        { 'id':         'B_B-I-U-1_SM',
          'desc':       'query the "bold" state, bold tag not immediate parent',
          'pad':        '<b>foo<i>ba[r<u>b]az</u></i></strong>',
          'expected':   True },

        { 'id':         'B_TEXT-B-1_SO-1',
          'desc':       'query the "bold" state, selection wrapping the bold tag',
          'pad':        'foo[<b>bar</b>]baz',
          'expected':   True },

        { 'id':         'B_TEXT-B-1_SO-2',
          'desc':       'query the "bold" state, selection wrapping the bold tag',
          'pad':        'foo{<b>bar</b>}baz',
          'expected':   True },

        { 'id':         'B_TEXT-B-1_SL',
          'desc':       'query the "bold" state, selection containing non-bold text',
          'pad':        'fo[o<b>ba]r</b>baz',
          'expected':   False },

        { 'id':         'B_TEXT-B-1_SR',
          'desc':       'query the "bold" state, selection containing non-bold text',
          'pad':        'foo<b>b[ar</b>b]az',
          'expected':   False },

        { 'id':         'B_TEXT-B-1_SO-3',
          'desc':       'query the "bold" state, selection containing non-bold text',
          'pad':        'fo[o<b>bar</b>b]az',
          'expected':   False },

        { 'id':         'B_B.TEXT.B-1_SM',
          'desc':       'query the "bold" state, selection including non-bold text',
          'pad':        '<b>fo[o</b>bar<b>b]az</b>',
          'expected':   False },

        { 'id':         'B_B.B.B-1_SM',
          'desc':       'query the "bold" state, selection mixed, but all bold',
          'pad':        '<b>fo[o</b><b>bar</b><b>b]az</b>',
          'expected':   True },

        { 'id':         'B_B.STRONG.B-1_SM',
          'desc':       'query the "bold" state, selection mixed, but all bold',
          'pad':        '<b>fo[o</b><strong>bar</strong><b>b]az</b>',
          'expected':   True },

        { 'id':         'B_SPAN.b.MYB.SPANs:fw:b-1_SM',
          'desc':       'query the "bold" state, selection mixed, but all bold',
          'pad':        '<span class="b">fo[o</span><myb>bar</myb><span style="font-weight: bold">b]az</span>',
          'expected':   True }
      ]
    },

    { 'desc':       'query italic state',
      'qcstate':    'italic',
      'tests':      [
        { 'id':         'I_TEXT_SI',
          'rte1-id':    'q-italic-0',
          'desc':       'query the "italic" state',
          'pad':        'foo[bar]baz',
          'expected':   False },

        { 'id':         'I_I-1_SI',
          'rte1-id':    'q-italic-1',
          'desc':       'query the "italic" state',
          'pad':        '<i>foo[bar]baz</i>',
          'expected':   True },

        { 'id':         'I_EM-1_SI',
          'rte1-id':    'q-italic-2',
          'desc':       'query the "italic" state',
          'pad':        '<em>foo[bar]baz</em>',
          'expected':   True },

        { 'id':         'I_SPANs:fs:i-1_SI',
          'rte1-id':    'q-italic-3',
          'desc':       'query the "italic" state',
          'pad':        '<span style="font-style: italic">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'I_SPANs:fs:n-1_SI',
          'desc':       'query the "italic" state',
          'pad':        '<span style="font-style: normal">foo[bar]baz</span>',
          'expected':   False },

        { 'id':         'I_I-SPANs:fs:n-1_SI',
          'rte1-id':    'q-italic-4',
          'desc':       'query the "italic" state',
          'pad':        '<i><span style="font-style: normal">foo[bar]baz</span></i>',
          'expected':   False },

        { 'id':         'I_SPAN.i-1-SI',
          'desc':       'query the "italic" state',
          'pad':        '<span class="i">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'I_MYI-1-SI',
          'desc':       'query the "italic" state',
          'pad':        '<myi>foo[bar]baz</myi>',
          'expected':   True }
      ]
    },

    { 'desc':       'query underline state',
      'qcstate':    'underline',
      'tests':      [
        { 'id':         'U_TEXT_SI',
          'rte1-id':    'q-underline-0',
          'desc':       'query the "underline" state',
          'pad':        'foo[bar]baz',
          'expected':   False },

        { 'id':         'U_U-1_SI',
          'rte1-id':    'q-underline-1',
          'desc':       'query the "underline" state',
          'pad':        '<u>foo[bar]baz</u>',
          'expected':   True },

        { 'id':         'U_Us:td:n-1_SI',
          'rte1-id':    'q-underline-4',
          'desc':       'query the "underline" state',
          'pad':        '<u style="text-decoration: none">foo[bar]baz</u>',
          'expected':   False },

        { 'id':         'U_Ah:url-1_SI',
          'rte1-id':    'q-underline-2',
          'desc':       'query the "underline" state',
          'pad':        '<a href="http://www.goo.gl">foo[bar]baz</a>',
          'expected':   True },

        { 'id':         'U_Ah:url.s:td:n-1_SI',
          'rte1-id':    'q-underline-5',
          'desc':       'query the "underline" state',
          'pad':        '<a href="http://www.goo.gl" style="text-decoration: none">foo[bar]baz</a>',
          'expected':   False },

        { 'id':         'U_SPANs:td:u-1_SI',
          'rte1-id':    'q-underline-3',
          'desc':       'query the "underline" state',
          'pad':        '<span style="text-decoration: underline">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'U_SPAN.u-1-SI',
          'desc':       'query the "underline" state',
          'pad':        '<span class="u">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'U_MYU-1-SI',
          'desc':       'query the "underline" state',
          'pad':        '<myu>foo[bar]baz</myu>',
          'expected':   True }
      ]
    },

    { 'desc':       'query strike-through state',
      'qcstate':    'strikethrough',
      'tests':      [
        { 'id':         'S_TEXT_SI',
          'rte1-id':    'q-strikethrough-0',
          'desc':       'query the "strikethrough" state',
          'pad':        'foo[bar]baz',
          'expected':   False },

        { 'id':         'S_S-1_SI',
          'rte1-id':    'q-strikethrough-3',
          'desc':       'query the "strikethrough" state',
          'pad':        '<s>foo[bar]baz</s>',
          'expected':   True },

        { 'id':         'S_STRIKE-1_SI',
          'rte1-id':    'q-strikethrough-1',
          'desc':       'query the "strikethrough" state',
          'pad':        '<strike>foo[bar]baz</strike>',
          'expected':   True },

        { 'id':         'S_STRIKEs:td:n-1_SI',
          'rte1-id':    'q-strikethrough-2',
          'desc':       'query the "strikethrough" state',
          'pad':        '<strike style="text-decoration: none">foo[bar]baz</strike>',
          'expected':   False },

        { 'id':         'S_DEL-1_SI',
          'rte1-id':    'q-strikethrough-4',
          'desc':       'query the "strikethrough" state',
          'pad':        '<del>foo[bar]baz</del>',
          'expected':   True },

        { 'id':         'S_SPANs:td:lt-1_SI',
          'rte1-id':    'q-strikethrough-5',
          'desc':       'query the "strikethrough" state',
          'pad':        '<span style="text-decoration: line-through">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'S_SPAN.s-1-SI',
          'desc':       'query the "strikethrough" state',
          'pad':        '<span class="s">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'S_MYS-1-SI',
          'desc':       'query the "strikethrough" state',
          'pad':        '<mys>foo[bar]baz</mys>',
          'expected':   True },

        { 'id':         'S_S.STRIKE.DEL-1_SM',
          'desc':       'query the "strikethrough" state, selection mixed, but all struck',
          'pad':        '<s>fo[o</s><strike>bar</strike><del>b]az</del>',
          'expected':   True }
      ]
    },

    { 'desc':       'query subscript state',
      'qcstate':    'subscript',
      'tests':      [
        { 'id':         'SUB_TEXT_SI',
          'rte1-id':    'q-subscript-0',
          'desc':       'query the "subscript" state',
          'pad':        'foo[bar]baz',
          'expected':   False },

        { 'id':         'SUB_SUB-1_SI',
          'rte1-id':    'q-subscript-1',
          'desc':       'query the "subscript" state',
          'pad':        '<sub>foo[bar]baz</sub>',
          'expected':   True },

        { 'id':         'SUB_SPAN.sub-1-SI',
          'desc':       'query the "subscript" state',
          'pad':        '<span class="sub">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'SUB_MYSUB-1-SI',
          'desc':       'query the "subscript" state',
          'pad':        '<mysub>foo[bar]baz</mysub>',
          'expected':   True }
      ]
    },

    { 'desc':       'query superscript state',
      'qcstate':    'superscript',
      'tests':      [
        { 'id':         'SUP_TEXT_SI',
          'rte1-id':    'q-superscript-0',
          'desc':       'query the "superscript" state',
          'pad':        'foo[bar]baz',
          'expected':   False },

        { 'id':         'SUP_SUP-1_SI',
          'rte1-id':    'q-superscript-1',
          'desc':       'query the "superscript" state',
          'pad':        '<sup>foo[bar]baz</sup>',
          'expected':   True },

        { 'id':         'IOL_TEXT_SI',
          'desc':       'query the "insertorderedlist" state',
          'pad':        'foo[bar]baz',
          'expected':   False },

        { 'id':         'SUP_SPAN.sup-1-SI',
          'desc':       'query the "superscript" state',
          'pad':        '<span class="sup">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'SUP_MYSUP-1-SI',
          'desc':       'query the "superscript" state',
          'pad':        '<mysup>foo[bar]baz</mysup>',
          'expected':   True }
      ]
    },

    { 'desc':       'query whether the selection is in an ordered list',
      'qcstate':    'insertorderedlist',
      'tests':      [
        { 'id':         'IOL_TEXT-1_SI',
          'rte1-id':    'q-insertorderedlist-0',
          'desc':       'query the "insertorderedlist" state',
          'pad':        'foo[bar]baz',
          'expected':   False },

        { 'id':         'IOL_OL-LI-1_SI',
          'rte1-id':    'q-insertorderedlist-1',
          'desc':       'query the "insertorderedlist" state',
          'pad':        '<ol><li>foo[bar]baz</li></ol>',
          'expected':   True },

        { 'id':         'IOL_UL_LI-1_SI',
          'rte1-id':    'q-insertorderedlist-2',
          'desc':       'query the "insertorderedlist" state',
          'pad':        '<ul><li>foo[bar]baz</li></ul>',
          'expected':   False }
      ]
    },

    { 'desc':       'query whether the selection is in an unordered list',
      'qcstate':    'insertunorderedlist',
      'tests':      [
        { 'id':         'IUL_TEXT_SI',
          'rte1-id':    'q-insertunorderedlist-0',
          'desc':       'query the "insertunorderedlist" state',
          'pad':        'foo[bar]baz',
          'expected':   False },

        { 'id':         'IUL_OL-LI-1_SI',
          'rte1-id':    'q-insertunorderedlist-1',
          'desc':       'query the "insertunorderedlist" state',
          'pad':        '<ol><li>foo[bar]baz</li></ol>',
          'expected':   False },

        { 'id':         'IUL_UL-LI-1_SI',
          'rte1-id':    'q-insertunorderedlist-2',
          'desc':       'query the "insertunorderedlist" state',
          'pad':        '<ul><li>foo[bar]baz</li></ul>',
          'expected':   True }
      ]
    },

    { 'desc':       'query whether the paragraph is centered',
      'qcstate':    'justifycenter',
      'tests':      [
        { 'id':         'JC_TEXT_SI',
          'rte1-id':    'q-justifycenter-0',
          'desc':       'query the "justifycenter" state',
          'pad':        'foo[bar]baz',
          'expected':   False },

        { 'id':         'JC_DIVa:c-1_SI',
          'rte1-id':    'q-justifycenter-1',
          'desc':       'query the "justifycenter" state',
          'pad':        '<div align="center">foo[bar]baz</div>',
          'expected':   True },

        { 'id':         'JC_Pa:c-1_SI',
          'rte1-id':    'q-justifycenter-2',
          'desc':       'query the "justifycenter" state',
          'pad':        '<p align="center">foo[bar]baz</p>',
          'expected':   True },

        { 'id':         'JC_SPANs:ta:c-1_SI',
          'rte1-id':    'q-justifycenter-3',
          'desc':       'query the "justifycenter" state',
          'pad':        '<div style="text-align: center">foo[bar]baz</div>',
          'expected':   True },

        { 'id':         'JC_SPAN.jc-1-SI',
          'desc':       'query the "justifycenter" state',
          'pad':        '<span class="jc">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'JC_MYJC-1-SI',
          'desc':       'query the "justifycenter" state',
          'pad':        '<myjc>foo[bar]baz</myjc>',
          'expected':   True }
      ]
    },

    { 'desc':       'query whether the paragraph is justified',
      'qcstate':    'justifyfull',
      'tests':      [
        { 'id':         'JF_TEXT_SI',
          'rte1-id':    'q-justifyfull-0',
          'desc':       'query the "justifyfull" state',
          'pad':        'foo[bar]baz',
          'expected':   False },

        { 'id':         'JF_DIVa:j-1_SI',
          'rte1-id':    'q-justifyfull-1',
          'desc':       'query the "justifyfull" state',
          'pad':        '<div align="justify">foo[bar]baz</div>',
          'expected':   True },

        { 'id':         'JF_Pa:j-1_SI',
          'rte1-id':    'q-justifyfull-2',
          'desc':       'query the "justifyfull" state',
          'pad':        '<p align="justify">foo[bar]baz</p>',
          'expected':   True },

        { 'id':         'JF_SPANs:ta:j-1_SI',
          'rte1-id':    'q-justifyfull-3',
          'desc':       'query the "justifyfull" state',
          'pad':        '<span style="text-align: justify">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'JF_SPAN.jf-1-SI',
          'desc':       'query the "justifyfull" state',
          'pad':        '<span class="jf">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'JF_MYJF-1-SI',
          'desc':       'query the "justifyfull" state',
          'pad':        '<myjf>foo[bar]baz</myjf>',
          'expected':   True }
      ]
    },

    { 'desc':       'query whether the paragraph is aligned left',
      'qcstate':    'justifyleft',
      'tests':      [
        { 'id':         'JL_TEXT_SI',
          'desc':       'query the "justifyleft" state',
          'pad':        'foo[bar]baz',
          'expected':   False },

        { 'id':         'JL_DIVa:l-1_SI',
          'rte1-id':    'q-justifyleft-0',
          'desc':       'query the "justifyleft" state',
          'pad':        '<div align="left">foo[bar]baz</div>',
          'expected':   True },

        { 'id':         'JL_Pa:l-1_SI',
          'rte1-id':    'q-justifyleft-1',
          'desc':       'query the "justifyleft" state',
          'pad':        '<p align="left">foo[bar]baz</p>',
          'expected':   True },

        { 'id':         'JL_SPANs:ta:l-1_SI',
          'rte1-id':    'q-justifyleft-2',
          'desc':       'query the "justifyleft" state',
          'pad':        '<span style="text-align: left">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'JL_SPAN.jl-1-SI',
          'desc':       'query the "justifyleft" state',
          'pad':        '<span class="jl">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'JL_MYJL-1-SI',
          'desc':       'query the "justifyleft" state',
          'pad':        '<myjl>foo[bar]baz</myjl>',
          'expected':   True }
      ]
    },

    { 'desc':       'query whether the paragraph is aligned right',
      'qcstate':    'justifyright',
      'tests':      [
        { 'id':         'JR_TEXT_SI',
          'rte1-id':    'q-justifyright-0',
          'desc':       'query the "justifyright" state',
          'pad':        'foo[bar]baz',
          'expected':   False },

        { 'id':         'JR_DIVa:r-1_SI',
          'rte1-id':    'q-justifyright-1',
          'desc':       'query the "justifyright" state',
          'pad':        '<div align="right">foo[bar]baz</div>',
          'expected':   True },

        { 'id':         'JR_Pa:r-1_SI',
          'rte1-id':    'q-justifyright-2',
          'desc':       'query the "justifyright" state',
          'pad':        '<p align="right">foo[bar]baz</p>',
          'expected':   True },

        { 'id':         'JR_SPANs:ta:r-1_SI',
          'rte1-id':    'q-justifyright-3',
          'desc':       'query the "justifyright" state',
          'pad':        '<span style="text-align: right">foo[bar]baz</span>',
          'expected':   True },

        { 'id':         'JR_SPAN.jr-1-SI',
          'desc':       'query the "justifyright" state',
          'pad':        '<span class="jr">foo[bar]baz</span>',
          'expected':   True },
          
        { 'id':         'JR_MYJR-1-SI',
          'desc':       'query the "justifyright" state',
          'pad':        '<myjr>foo[bar]baz</myjr>',
          'expected':   True }
      ]
    }
  ]
}

QUERYSTATE_TESTS_CSS = {
  'id':           'QSC',
  'caption':      'queryCommandState Tests, using styleWithCSS',
  'checkAttrs':   False,
  'checkStyle':   False,
  'styleWithCSS': True,
  
  'Proposed':     QUERYSTATE_TESTS['Proposed']
}

