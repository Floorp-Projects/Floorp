
QUERYSUPPORTED_TESTS = {
  'id':            'Q',
  'caption':       'queryCommandSupported Tests',
  'pad':           'foo[bar]baz',
  'checkAttrs':    False,
  'checkStyle':    False,
  'styleWithCSS':  False,
  'expected':      True,

  'Proposed': [
    { 'desc':       '',
      'tests':      [
      ]
    },

    { 'desc':       'HTML5 commands',
      'tests':      [
        { 'id':          'SELECTALL_TEXT-1',
          'desc':        'check whether the "selectall" command is supported',
          'qcsupported': 'selectall' },

        { 'id':          'UNSELECT_TEXT-1',
          'desc':        'check whether the "unselect" command is supported',
          'qcsupported': 'unselect' },

        { 'id':          'UNDO_TEXT-1',
          'desc':        'check whether the "undo" command is supported',
          'qcsupported': 'undo' },

        { 'id':          'REDO_TEXT-1',
          'desc':        'check whether the "redo" command is supported',
          'qcsupported': 'redo' },

        { 'id':          'BOLD_TEXT-1',
          'desc':        'check whether the "bold" command is supported',
          'qcsupported': 'bold' },

        { 'id':          'BOLD_B',
          'desc':        'check whether the "bold" command is supported',
          'qcsupported': 'bold',
          'pad':         '<b>foo[bar]baz</b>' },

        { 'id':          'ITALIC_TEXT-1',
          'desc':        'check whether the "italic" command is supported',
          'qcsupported': 'italic' },

        { 'id':          'ITALIC_I',
          'desc':        'check whether the "italic" command is supported',
          'qcsupported': 'italic',
          'pad':         '<i>foo[bar]baz</i>' },

        { 'id':          'UNDERLINE_TEXT-1',
          'desc':        'check whether the "underline" command is supported',
          'qcsupported': 'underline' },

        { 'id':          'STRIKETHROUGH_TEXT-1',
          'desc':        'check whether the "strikethrough" command is supported',
          'qcsupported': 'strikethrough' },

        { 'id':          'SUBSCRIPT_TEXT-1',
          'desc':        'check whether the "subscript" command is supported',
          'qcsupported': 'subscript' },

        { 'id':          'SUPERSCRIPT_TEXT-1',
          'desc':        'check whether the "superscript" command is supported',
          'qcsupported': 'superscript' },

        { 'id':          'FORMATBLOCK_TEXT-1',
          'desc':        'check whether the "formatblock" command is supported',
          'qcsupported': 'formatblock' },

        { 'id':          'CREATELINK_TEXT-1',
          'desc':        'check whether the "createlink" command is supported',
          'qcsupported': 'createlink' },

        { 'id':          'UNLINK_TEXT-1',
          'desc':        'check whether the "unlink" command is supported',
          'qcsupported': 'unlink' },

        { 'id':          'INSERTHTML_TEXT-1',
          'desc':        'check whether the "inserthtml" command is supported',
          'qcsupported': 'inserthtml' },

        { 'id':          'INSERTHORIZONTALRULE_TEXT-1',
          'desc':        'check whether the "inserthorizontalrule" command is supported',
          'qcsupported': 'inserthorizontalrule' },

        { 'id':          'INSERTIMAGE_TEXT-1',
          'desc':        'check whether the "insertimage" command is supported',
          'qcsupported': 'insertimage' },

        { 'id':          'INSERTLINEBREAK_TEXT-1',
          'desc':        'check whether the "insertlinebreak" command is supported',
          'qcsupported': 'insertlinebreak' },

        { 'id':          'INSERTPARAGRAPH_TEXT-1',
          'desc':        'check whether the "insertparagraph" command is supported',
          'qcsupported': 'insertparagraph' },

        { 'id':          'INSERTORDEREDLIST_TEXT-1',
          'desc':        'check whether the "insertorderedlist" command is supported',
          'qcsupported': 'insertorderedlist' },

        { 'id':          'INSERTUNORDEREDLIST_TEXT-1',
          'desc':        'check whether the "insertunorderedlist" command is supported',
          'qcsupported': 'insertunorderedlist' },

        { 'id':          'INSERTTEXT_TEXT-1',
          'desc':        'check whether the "inserttext" command is supported',
          'qcsupported': 'inserttext' },

        { 'id':          'DELETE_TEXT-1',
          'desc':        'check whether the "delete" command is supported',
          'qcsupported': 'delete' },

        { 'id':          'FORWARDDELETE_TEXT-1',
          'desc':        'check whether the "forwarddelete" command is supported',
          'qcsupported': 'forwarddelete' }
      ]
    },

    { 'desc':       'MIDAS commands',
      'tests':      [
        { 'id':          'STYLEWITHCSS_TEXT-1',
          'desc':        'check whether the "styleWithCSS" command is supported',
          'qcsupported': 'styleWithCSS' },

        { 'id':          'CONTENTREADONLY_TEXT-1',
          'desc':        'check whether the "contentreadonly" command is supported',
          'qcsupported': 'contentreadonly' },

        { 'id':          'BACKCOLOR_TEXT-1',
          'desc':        'check whether the "backcolor" command is supported',
          'qcsupported': 'backcolor' },

        { 'id':          'FORECOLOR_TEXT-1',
          'desc':        'check whether the "forecolor" command is supported',
          'qcsupported': 'forecolor' },

        { 'id':          'HILITECOLOR_TEXT-1',
          'desc':        'check whether the "hilitecolor" command is supported',
          'qcsupported': 'hilitecolor' },

        { 'id':          'FONTNAME_TEXT-1',
          'desc':        'check whether the "fontname" command is supported',
          'qcsupported': 'fontname' },

        { 'id':          'FONTSIZE_TEXT-1',
          'desc':        'check whether the "fontsize" command is supported',
          'qcsupported': 'fontsize' },

        { 'id':          'INCREASEFONTSIZE_TEXT-1',
          'desc':        'check whether the "increasefontsize" command is supported',
          'qcsupported': 'increasefontsize' },

        { 'id':          'DECREASEFONTSIZE_TEXT-1',
          'desc':        'check whether the "decreasefontsize" command is supported',
          'qcsupported': 'decreasefontsize' },

        { 'id':          'HEADING_TEXT-1',
          'desc':        'check whether the "heading" command is supported',
          'qcsupported': 'heading' },

        { 'id':          'INDENT_TEXT-1',
          'desc':        'check whether the "indent" command is supported',
          'qcsupported': 'indent' },

        { 'id':          'OUTDENT_TEXT-1',
          'desc':        'check whether the "outdent" command is supported',
          'qcsupported': 'outdent' },

        { 'id':          'CREATEBOOKMARK_TEXT-1',
          'desc':        'check whether the "createbookmark" command is supported',
          'qcsupported': 'createbookmark' },

        { 'id':          'UNBOOKMARK_TEXT-1',
          'desc':        'check whether the "unbookmark" command is supported',
          'qcsupported': 'unbookmark' },

        { 'id':          'JUSTIFYCENTER_TEXT-1',
          'desc':        'check whether the "justifycenter" command is supported',
          'qcsupported': 'justifycenter' },

        { 'id':          'JUSTIFYFULL_TEXT-1',
          'desc':        'check whether the "justifyfull" command is supported',
          'qcsupported': 'justifyfull' },

        { 'id':          'JUSTIFYLEFT_TEXT-1',
          'desc':        'check whether the "justifyleft" command is supported',
          'qcsupported': 'justifyleft' },

        { 'id':          'JUSTIFYRIGHT_TEXT-1',
          'desc':        'check whether the "justifyright" command is supported',
          'qcsupported': 'justifyright' },

        { 'id':          'REMOVEFORMAT_TEXT-1',
          'desc':        'check whether the "removeformat" command is supported',
          'qcsupported': 'removeformat' },

        { 'id':          'COPY_TEXT-1',
          'desc':        'check whether the "copy" command is supported',
          'qcsupported': 'copy' },

        { 'id':          'CUT_TEXT-1',
          'desc':        'check whether the "cut" command is supported',
          'qcsupported': 'cut' },

        { 'id':          'PASTE_TEXT-1',
          'desc':        'check whether the "paste" command is supported',
          'qcsupported': 'paste' }
      ]
    },
    
    { 'desc':       'Other tests',
      'tests':      [
        { 'id':          'garbage-1_TEXT-1',
          'desc':        'check correct return value with garbage input',
          'qcsupported': '#!#@7',
          'expected':    False }
      ]
    }
  ]
}


