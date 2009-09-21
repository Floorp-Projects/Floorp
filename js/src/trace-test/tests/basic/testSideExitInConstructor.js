// |trace-test| TMFLAGS: full,fragprofile,treevis; valgrind

function testSideExitInConstructor() {
    var FCKConfig = {};
    FCKConfig.CoreStyles =
    {
        'Bold': { },
        'Italic': { },
        'FontFace': { },
        'Size' :
        {
        Overrides: [ ]
        },

        'Color' :
        {
        Element: '',
        Styles: {  },
        Overrides: [  ]
        },
        'BackColor': {
        Element : '',
        Styles : { 'background-color' : '' }
        },

    };
    var FCKStyle = function(A) {
        A.Element;
    };

    var pass = true;
    for (var s in FCKConfig.CoreStyles) {
        var x = new FCKStyle(FCKConfig.CoreStyles[s]);
        if (!x)
            pass = false;
    }
    return pass;
}
assertEq(testSideExitInConstructor(), true);
