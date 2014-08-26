
QUERYENABLED_TESTS = {
  'id':           'QE',
  'caption':      'queryCommandEnabled Tests',
  'pad':          'foo[bar]baz',
  'checkAttrs':   False,
  'checkStyle':   False,
  'styleWithCSS': False,
  'expected':     True,

  'Proposed': [
    { 'desc':       '',
      'tests':      [
      ]
    },

    { 'desc':       'HTML5 commands',
      'tests':      [
        { 'id':         'SELECTALL_TEXT-1',
          'desc':       'check whether the "selectall" command is enabled',
          'qcenabled':  'selectall' },

        { 'id':         'UNSELECT_TEXT-1',
          'desc':       'check whether the "unselect" command is enabled',
          'qcenabled':  'unselect' },

        { 'id':         'UNDO_TEXT-1',
          'desc':       'check whether the "undo" command is enabled',
          'qcenabled':  'undo' },

        { 'id':         'REDO_TEXT-1',
          'desc':       'check whether the "redo" command is enabled',
          'qcenabled':  'redo' },

        { 'id':         'BOLD_TEXT-1',
          'desc':       'check whether the "bold" command is enabled',
          'qcenabled':  'bold' },

        { 'id':         'ITALIC_TEXT-1',
          'desc':       'check whether the "italic" command is enabled',
          'qcenabled':  'italic' },

        { 'id':         'UNDERLINE_TEXT-1',
          'desc':       'check whether the "underline" command is enabled',
          'qcenabled':  'underline' },

        { 'id':         'STRIKETHROUGH_TEXT-1',
          'desc':       'check whether the "strikethrough" command is enabled',
          'qcenabled':  'strikethrough' },

        { 'id':         'SUBSCRIPT_TEXT-1',
          'desc':       'check whether the "subscript" command is enabled',
          'qcenabled':  'subscript' },

        { 'id':         'SUPERSCRIPT_TEXT-1',
          'desc':       'check whether the "superscript" command is enabled',
          'qcenabled':  'superscript' },
          
        { 'id':         'FORMATBLOCK_TEXT-1',
          'desc':       'check whether the "formatblock" command is enabled',
          'qcenabled':  'formatblock' },

        { 'id':         'CREATELINK_TEXT-1',
          'desc':       'check whether the "createlink" command is enabled',
          'qcenabled':  'createlink' },
          
        { 'id':         'UNLINK_TEXT-1',
          'desc':       'check whether the "unlink" command is enabled',
          'qcenabled':  'unlink' },

        { 'id':         'INSERTHTML_TEXT-1',
          'desc':       'check whether the "inserthtml" command is enabled',
          'qcenabled':  'inserthtml' },

        { 'id':         'INSERTHORIZONTALRULE_TEXT-1',
          'desc':       'check whether the "inserthorizontalrule" command is enabled',
          'qcenabled':  'inserthorizontalrule' },

        { 'id':         'INSERTIMAGE_TEXT-1',
          'desc':       'check whether the "insertimage" command is enabled',
          'qcenabled':  'insertimage' },

        { 'id':         'INSERTLINEBREAK_TEXT-1',
          'desc':       'check whether the "insertlinebreak" command is enabled',
          'qcenabled':  'insertlinebreak' },

        { 'id':         'INSERTPARAGRAPH_TEXT-1',
          'desc':       'check whether the "insertparagraph" command is enabled',
          'qcenabled':  'insertparagraph' },

        { 'id':         'INSERTORDEREDLIST_TEXT-1',
          'desc':       'check whether the "insertorderedlist" command is enabled',
          'qcenabled':  'insertorderedlist' },

        { 'id':         'INSERTUNORDEREDLIST_TEXT-1',
          'desc':       'check whether the "insertunorderedlist" command is enabled',
          'qcenabled':  'insertunorderedlist' },

        { 'id':         'INSERTTEXT_TEXT-1',
          'desc':       'check whether the "inserttext" command is enabled',
          'qcenabled':  'inserttext' },

        { 'id':         'DELETE_TEXT-1',
          'desc':       'check whether the "delete" command is enabled',
          'qcenabled':  'delete' },

        { 'id':         'FORWARDDELETE_TEXT-1',
          'desc':       'check whether the "forwarddelete" command is enabled',
          'qcenabled':  'forwarddelete' }
      ]
    },

    { 'desc':       'MIDAS commands',
      'tests':      [
        { 'id':         'STYLEWITHCSS_TEXT-1',
          'desc':       'check whether the "styleWithCSS" command is enabled',
          'qcenabled':  'styleWithCSS' },

        { 'id':         'CONTENTREADONLY_TEXT-1',
          'desc':       'check whether the "contentreadonly" command is enabled',
          'qcenabled':  'contentreadonly' },

        { 'id':         'BACKCOLOR_TEXT-1',
          'desc':       'check whether the "backcolor" command is enabled',
          'qcenabled':  'backcolor' },

        { 'id':         'FORECOLOR_TEXT-1',
          'desc':       'check whether the "forecolor" command is enabled',
          'qcenabled':  'forecolor' },

        { 'id':         'HILITECOLOR_TEXT-1',
          'desc':       'check whether the "hilitecolor" command is enabled',
          'qcenabled':  'hilitecolor' },

        { 'id':         'FONTNAME_TEXT-1',
          'desc':       'check whether the "fontname" command is enabled',
          'qcenabled':  'fontname' },

        { 'id':         'FONTSIZE_TEXT-1',
          'desc':       'check whether the "fontsize" command is enabled',
          'qcenabled':  'fontsize' },

        { 'id':         'INCREASEFONTSIZE_TEXT-1',
          'desc':       'check whether the "increasefontsize" command is enabled',
          'qcenabled':  'increasefontsize' },

        { 'id':         'DECREASEFONTSIZE_TEXT-1',
          'desc':       'check whether the "decreasefontsize" command is enabled',
          'qcenabled':  'decreasefontsize' },

        { 'id':         'HEADING_TEXT-1',
          'desc':       'check whether the "heading" command is enabled',
          'qcenabled':  'heading' },

        { 'id':         'INDENT_TEXT-1',
          'desc':       'check whether the "indent" command is enabled',
          'qcenabled':  'indent' },

        { 'id':         'OUTDENT_TEXT-1',
          'desc':       'check whether the "outdent" command is enabled',
          'qcenabled':  'outdent' },

        { 'id':         'CREATEBOOKMARK_TEXT-1',
          'desc':       'check whether the "createbookmark" command is enabled',
          'qcenabled':  'createbookmark' },

        { 'id':         'UNBOOKMARK_TEXT-1',
          'desc':       'check whether the "unbookmark" command is enabled',
          'qcenabled':  'unbookmark' },

        { 'id':         'JUSTIFYCENTER_TEXT-1',
          'desc':       'check whether the "justifycenter" command is enabled',
          'qcenabled':  'justifycenter' },

        { 'id':         'JUSTIFYFULL_TEXT-1',
          'desc':       'check whether the "justifyfull" command is enabled',
          'qcenabled':  'justifyfull' },

        { 'id':         'JUSTIFYLEFT_TEXT-1',
          'desc':       'check whether the "justifyleft" command is enabled',
          'qcenabled':  'justifyleft' },

        { 'id':         'JUSTIFYRIGHT_TEXT-1',
          'desc':       'check whether the "justifyright" command is enabled',
          'qcenabled':  'justifyright' },

        { 'id':         'REMOVEFORMAT_TEXT-1',
          'desc':       'check whether the "removeformat" command is enabled',
          'qcenabled':  'removeformat' },

        { 'id':         'COPY_TEXT-1',
          'desc':       'check whether the "copy" command is enabled',
          'qcenabled':  'copy' },

        { 'id':         'CUT_TEXT-1',
          'desc':       'check whether the "cut" command is enabled',
          'qcenabled':  'cut' },

        { 'id':         'PASTE_TEXT-1',
          'desc':       'check whether the "paste" command is enabled',
          'qcenabled':  'paste' }
      ]
    },

    { 'desc':       'Other tests',
      'tests':      [
        { 'id':         'garbage-1_TEXT-1',
          'desc':       'check correct return value with garbage input',
          'qcenabled':  '#!#@7',
          'expected':   False }
      ]
    }
  ]
}

