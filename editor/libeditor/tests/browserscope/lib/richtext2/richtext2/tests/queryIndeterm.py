
QUERYINDETERM_TESTS = {
  'id':           'QI',
  'caption':      'queryCommandIndeterm Tests',
  'pad':          'foo[bar]baz',
  'checkAttrs':   False,
  'checkStyle':   False,
  'styleWithCSS': False,
  'expected':     False,

  'Proposed': [
    { 'desc':       '',
      'tests':      [
      ]
    },

    { 'desc':       'HTML5 commands',
      'tests':      [
        { 'id':         'SELECTALL_TEXT-1',
          'desc':       'check whether the "selectall" command is indeterminate',
          'qcindeterm': 'selectall' },

        { 'id':         'UNSELECT_TEXT-1',
          'desc':       'check whether the "unselect" command is indeterminate',
          'qcindeterm': 'unselect' },

        { 'id':         'UNDO_TEXT-1',
          'desc':       'check whether the "undo" command is indeterminate',
          'qcindeterm': 'undo' },

        { 'id':         'REDO_TEXT-1',
          'desc':       'check whether the "redo" command is indeterminate',
          'qcindeterm': 'redo' },

        { 'id':         'BOLD_TEXT-1',
          'desc':       'check whether the "bold" command is indeterminate',
          'qcindeterm': 'bold' },

        { 'id':         'ITALIC_TEXT-1',
          'desc':       'check whether the "italic" command is indeterminate',
          'qcindeterm': 'italic' },

        { 'id':         'UNDERLINE_TEXT-1',
          'desc':       'check whether the "underline" command is indeterminate',
          'qcindeterm': 'underline' },

        { 'id':         'STRIKETHROUGH_TEXT-1',
          'desc':       'check whether the "strikethrough" command is indeterminate',
          'qcindeterm': 'strikethrough' },

        { 'id':         'SUBSCRIPT_TEXT-1',
          'desc':       'check whether the "subscript" command is indeterminate',
          'qcindeterm': 'subscript' },

        { 'id':         'SUPERSCRIPT_TEXT-1',
          'desc':       'check whether the "superscript" command is indeterminate',
          'qcindeterm': 'superscript' },

        { 'id':         'FORMATBLOCK_TEXT-1',
          'desc':       'check whether the "formatblock" command is indeterminate',
          'qcindeterm': 'formatblock' },

        { 'id':         'CREATELINK_TEXT-1',
          'desc':       'check whether the "createlink" command is indeterminate',
          'qcindeterm': 'createlink' },

        { 'id':         'UNLINK_TEXT-1',
          'desc':       'check whether the "unlink" command is indeterminate',
          'qcindeterm': 'unlink' },

        { 'id':         'INSERTHTML_TEXT-1',
          'desc':       'check whether the "inserthtml" command is indeterminate',
          'qcindeterm': 'inserthtml' },

        { 'id':         'INSERTHORIZONTALRULE_TEXT-1',
          'desc':       'check whether the "inserthorizontalrule" command is indeterminate',
          'qcindeterm': 'inserthorizontalrule' },

        { 'id':         'INSERTIMAGE_TEXT-1',
          'desc':       'check whether the "insertimage" command is indeterminate',
          'qcindeterm': 'insertimage' },

        { 'id':         'INSERTLINEBREAK_TEXT-1',
          'desc':       'check whether the "insertlinebreak" command is indeterminate',
          'qcindeterm': 'insertlinebreak' },

        { 'id':         'INSERTPARAGRAPH_TEXT-1',
          'desc':       'check whether the "insertparagraph" command is indeterminate',
          'qcindeterm': 'insertparagraph' },

        { 'id':         'INSERTORDEREDLIST_TEXT-1',
          'desc':       'check whether the "insertorderedlist" command is indeterminate',
          'qcindeterm': 'insertorderedlist' },

        { 'id':         'INSERTUNORDEREDLIST_TEXT-1',
          'desc':       'check whether the "insertunorderedlist" command is indeterminate',
          'qcindeterm': 'insertunorderedlist' },

        { 'id':         'INSERTTEXT_TEXT-1',
          'desc':       'check whether the "inserttext" command is indeterminate',
          'qcindeterm': 'inserttext' },

        { 'id':         'DELETE_TEXT-1',
          'desc':       'check whether the "delete" command is indeterminate',
          'qcindeterm': 'delete' },

        { 'id':         'FORWARDDELETE_TEXT-1',
          'desc':       'check whether the "forwarddelete" command is indeterminate',
          'qcindeterm': 'forwarddelete' }
      ]
    },

    { 'desc':       'MIDAS commands',
      'tests':      [
        { 'id':         'STYLEWITHCSS_TEXT-1',
          'desc':       'check whether the "styleWithCSS" command is indeterminate',
          'qcindeterm': 'styleWithCSS' },

        { 'id':         'CONTENTREADONLY_TEXT-1',
          'desc':       'check whether the "contentreadonly" command is indeterminate',
          'qcindeterm': 'contentreadonly' },

        { 'id':         'BACKCOLOR_TEXT-1',
          'desc':       'check whether the "backcolor" command is indeterminate',
          'qcindeterm': 'backcolor' },

        { 'id':         'FORECOLOR_TEXT-1',
          'desc':       'check whether the "forecolor" command is indeterminate',
          'qcindeterm': 'forecolor' },

        { 'id':         'HILITECOLOR_TEXT-1',
          'desc':       'check whether the "hilitecolor" command is indeterminate',
          'qcindeterm': 'hilitecolor' },

        { 'id':         'FONTNAME_TEXT-1',
          'desc':       'check whether the "fontname" command is indeterminate',
          'qcindeterm': 'fontname' },

        { 'id':         'FONTSIZE_TEXT-1',
          'desc':       'check whether the "fontsize" command is indeterminate',
          'qcindeterm': 'fontsize' },

        { 'id':         'INCREASEFONTSIZE_TEXT-1',
          'desc':       'check whether the "increasefontsize" command is indeterminate',
          'qcindeterm': 'increasefontsize' },

        { 'id':         'DECREASEFONTSIZE_TEXT-1',
          'desc':       'check whether the "decreasefontsize" command is indeterminate',
          'qcindeterm': 'decreasefontsize' },

        { 'id':         'HEADING_TEXT-1',
          'desc':       'check whether the "heading" command is indeterminate',
          'qcindeterm': 'heading' },

        { 'id':         'INDENT_TEXT-1',
          'desc':       'check whether the "indent" command is indeterminate',
          'qcindeterm': 'indent' },

        { 'id':         'OUTDENT_TEXT-1',
          'desc':       'check whether the "outdent" command is indeterminate',
          'qcindeterm': 'outdent' },

        { 'id':         'CREATEBOOKMARK_TEXT-1',
          'desc':       'check whether the "createbookmark" command is indeterminate',
          'qcindeterm': 'createbookmark' },

        { 'id':         'UNBOOKMARK_TEXT-1',
          'desc':       'check whether the "unbookmark" command is indeterminate',
          'qcindeterm': 'unbookmark' },

        { 'id':         'JUSTIFYCENTER_TEXT-1',
          'desc':       'check whether the "justifycenter" command is indeterminate',
          'qcindeterm': 'justifycenter' },

        { 'id':         'JUSTIFYFULL_TEXT-1',
          'desc':       'check whether the "justifyfull" command is indeterminate',
          'qcindeterm': 'justifyfull' },

        { 'id':         'JUSTIFYLEFT_TEXT-1',
          'desc':       'check whether the "justifyleft" command is indeterminate',
          'qcindeterm': 'justifyleft' },

        { 'id':         'JUSTIFYRIGHT_TEXT-1',
          'desc':       'check whether the "justifyright" command is indeterminate',
          'qcindeterm': 'justifyright' },

        { 'id':         'REMOVEFORMAT_TEXT-1',
          'desc':       'check whether the "removeformat" command is indeterminate',
          'qcindeterm': 'removeformat' },

        { 'id':         'COPY_TEXT-1',
          'desc':       'check whether the "copy" command is indeterminate',
          'qcindeterm': 'copy' },

        { 'id':         'CUT_TEXT-1',
          'desc':       'check whether the "cut" command is indeterminate',
          'qcindeterm':  'cut' },

        { 'id':         'PASTE_TEXT-1',
          'desc':       'check whether the "paste" command is indeterminate',
          'qcindeterm': 'paste' }
      ]
    },

    { 'desc':       'Other tests',
      'tests':      [
        { 'id':         'garbage-1_TEXT-1',
          'desc':       'check correct return value with garbage input',
          'qcindeterm': '#!#@7' }
      ]
    }
  ]
}

