
UNAPPLY_TESTS_CSS = {
  'id':            'UC',
  'caption':       'Unapply Existing Formatting Tests, using styleWithCSS',
  'checkAttrs':    True,
  'checkStyle':    True,
  'styleWithCSS':  True,
  'expected':      'foo[bar]baz',

  'Proposed': [
    { 'desc':       '',
      'id':         '',
      'command':    '',
      'tests':      [
      ]
    },

    { 'desc':       'remove bold',
      'command':    'bold',
      'tests':      [
        { 'id':         'B_B-1_SW',
          'desc':       'Selection within tags; remove <b> tags',
          'pad':        'foo<b>[bar]</b>baz' },

        { 'id':         'B_B-1_SO',
          'desc':       'Selection outside of tags; remove <b> tags',
          'pad':        'foo[<b>bar</b>]baz' },

        { 'id':         'B_B-1_SL',
          'desc':       'Selection oblique left; remove <b> tags',
          'pad':        'foo[<b>bar]</b>baz' },

        { 'id':         'B_B-1_SR',
          'desc':       'Selection oblique right; remove <b> tags',
          'pad':        'foo<b>[bar</b>]baz' },

        { 'id':         'B_STRONG-1_SW',
          'desc':       'Selection within tags; remove <strong> tags',
          'pad':        'foo<strong>[bar]</strong>baz' },

        { 'id':         'B_STRONG-1_SO',
          'desc':       'Selection outside of tags; remove <strong> tags',
          'pad':        'foo[<strong>bar</strong>]baz' },

        { 'id':         'B_STRONG-1_SL',
          'desc':       'Selection oblique left; remove <strong> tags',
          'pad':        'foo[<strong>bar]</strong>baz' },

        { 'id':         'B_STRONG-1_SR',
          'desc':       'Selection oblique right; remove <strong> tags',
          'pad':        'foo<strong>[bar</strong>]baz' },

        { 'id':         'B_SPANs:fw:b-1_SW',
          'desc':       'Selection within tags; remove "font-weight: bold"',
          'pad':        'foo<span style="font-weight: bold">[bar]</span>baz' },

        { 'id':         'B_SPANs:fw:b-1_SO',
          'desc':       'Selection outside of tags; remove "font-weight: bold"',
          'pad':        'foo[<span style="font-weight: bold">bar</span>]baz' },

        { 'id':         'B_SPANs:fw:b-1_SL',
          'desc':       'Selection oblique left; remove "font-weight: bold"',
          'pad':        'foo[<span style="font-weight: bold">bar]</span>baz' },

        { 'id':         'B_SPANs:fw:b-1_SR',
          'desc':       'Selection oblique right; remove "font-weight: bold"',
          'pad':        'foo<span style="font-weight: bold">[bar</span>]baz' }
      ]
    },

    { 'desc':       'remove italic',
      'command':    'italic',
      'tests':      [
        { 'id':         'I_I-1_SW',
          'desc':       'Selection within tags; remove <i> tags',
          'pad':        'foo<i>[bar]</i>baz' },

        { 'id':         'I_I-1_SO',
          'desc':       'Selection outside of tags; remove <i> tags',
          'pad':        'foo[<i>bar</i>]baz' },

        { 'id':         'I_I-1_SL',
          'desc':       'Selection oblique left; remove <i> tags',
          'pad':        'foo[<i>bar]</i>baz' },

        { 'id':         'I_I-1_SR',
          'desc':       'Selection oblique right; remove <i> tags',
          'pad':        'foo<i>[bar</i>]baz' },

        { 'id':         'I_EM-1_SW',
          'desc':       'Selection within tags; remove <em> tags',
          'pad':        'foo<em>[bar]</em>baz' },

        { 'id':         'I_EM-1_SO',
          'desc':       'Selection outside of tags; remove <em> tags',
          'pad':        'foo[<em>bar</em>]baz' },

        { 'id':         'I_EM-1_SL',
          'desc':       'Selection oblique left; remove <em> tags',
          'pad':        'foo[<em>bar]</em>baz' },

        { 'id':         'I_EM-1_SR',
          'desc':       'Selection oblique right; remove <em> tags',
          'pad':        'foo<em>[bar</em>]baz' },

        { 'id':         'I_SPANs:fs:i-1_SW',
          'desc':       'Selection within tags; remove "font-style: italic"',
          'pad':        'foo<span style="font-style: italic">[bar]</span>baz' },

        { 'id':         'I_SPANs:fs:i-1_SO',
          'desc':       'Selection outside of tags; Italicize "font-style: italic"',
          'pad':        'foo[<span style="font-style: italic">bar</span>]baz' },

        { 'id':         'I_SPANs:fs:i-1_SL',
          'desc':       'Selection oblique left; Italicize "font-style: italic"',
          'pad':        'foo[<span style="font-style: italic">bar]</span>baz' },

        { 'id':         'I_SPANs:fs:i-1_SR',
          'desc':       'Selection oblique right; Italicize "font-style: italic"',
          'pad':        'foo<span style="font-style: italic">[bar</span>]baz' }
      ]
    },

    { 'desc':       'remove underline',
      'command':    'underline',
      'tests':      [
        { 'id':         'U_U-1_SW',
          'desc':       'Selection within tags; remove <u> tags',
          'pad':        'foo<u>[bar]</u>baz' },

        { 'id':         'U_U-1_SO',
          'desc':       'Selection outside of tags; remove <u> tags',
          'pad':        'foo[<u>bar</u>]baz' },

        { 'id':         'U_U-1_SL',
          'desc':       'Selection oblique left; remove <u> tags',
          'pad':        'foo[<u>bar]</u>baz' },

        { 'id':         'U_U-1_SR',
          'desc':       'Selection oblique right; remove <u> tags',
          'pad':        'foo<u>[bar</u>]baz' },

        { 'id':         'U_SPANs:td:u-1_SW',
          'desc':       'Selection within tags; remove "text-decoration: underline"',
          'pad':        'foo<span style="text-decoration: underline">[bar]</span>baz' },

        { 'id':         'U_SPANs:td:u-1_SO',
          'desc':       'Selection outside of tags; remove "text-decoration: underline"',
          'pad':        'foo[<span style="text-decoration: underline">bar</span>]baz' },

        { 'id':         'U_SPANs:td:u-1_SL',
          'desc':       'Selection oblique left; remove "text-decoration: underline"',
          'pad':        'foo[<span style="text-decoration: underline">bar]</span>baz' },

        { 'id':         'U_SPANs:td:u-1_SR',
          'desc':       'Selection oblique right; remove "text-decoration: underline"',
          'pad':        'foo<span style="text-decoration: underline">[bar</span>]baz' }
      ]
    },

    { 'desc':       'remove strike-through',
      'command':    'strikethrough',
      'tests':      [
        { 'id':         'S_S-1_SW',
          'desc':       'Selection within tags; remove <s> tags',
          'pad':        'foo<s>[bar]</s>baz' },

        { 'id':         'S_S-1_SO',
          'desc':       'Selection outside of tags; remove <s> tags',
          'pad':        'foo[<s>bar</s>]baz' },

        { 'id':         'S_S-1_SL',
          'desc':       'Selection oblique left; remove <s> tags',
          'pad':        'foo[<s>bar]</s>baz' },

        { 'id':         'S_S-1_SR',
          'desc':       'Selection oblique right; remove <s> tags',
          'pad':        'foo<s>[bar</s>]baz' },

        { 'id':         'S_STRIKE-1_SW',
          'desc':       'Selection within tags; remove <strike> tags',
          'pad':        'foo<strike>[bar]</strike>baz' },

        { 'id':         'S_STRIKE-1_SO',
          'desc':       'Selection outside of tags; remove <strike> tags',
          'pad':        'foo[<strike>bar</strike>]baz' },

        { 'id':         'S_STRIKE-1_SL',
          'desc':       'Selection oblique left; remove <strike> tags',
          'pad':        'foo[<strike>bar]</strike>baz' },

        { 'id':         'S_STRIKE-1_SR',
          'desc':       'Selection oblique right; remove <strike> tags',
          'pad':        'foo<strike>[bar</strike>]baz' },

        { 'id':         'S_SPANs:td:lt-1_SW',
          'desc':       'Selection within tags; remove "text-decoration:line-through"',
          'pad':        'foo<span style="text-decoration:line-through">[bar]</span>baz' },

        { 'id':         'S_SPANs:td:lt-1_SO',
          'desc':       'Selection outside of tags; Italicize "text-decoration:line-through"',
          'pad':        'foo[<span style="text-decoration:line-through">bar</span>]baz' },

        { 'id':         'S_SPANs:td:lt-1_SL',
          'desc':       'Selection oblique left; Italicize "text-decoration:line-through"',
          'pad':        'foo[<span style="text-decoration:line-through">bar]</span>baz' },

        { 'id':         'S_SPANs:td:lt-1_SR',
          'desc':       'Selection oblique right; Italicize "text-decoration:line-through"',
          'pad':        'foo<span style="text-decoration:line-through">[bar</span>]baz' },
                
        { 'id':         'S_SPANc:s-1_SW',
          'desc':       'Unapply "strike-through" on interited CSS style',
          'checkClass': True,
          'pad':        'foo<span class="s">[bar]</span>baz' },

        { 'id':         'S_SPANc:s-2_SI',
          'desc':       'Unapply "strike-through" on interited CSS style',
          'pad':        '<span class="s">foo[bar]baz</span>',
          'checkClass': True,
          'expected':   '<span class="s">foo</span>[bar]<span class="s">baz</span>' }
      ]
    }
  ]
}

