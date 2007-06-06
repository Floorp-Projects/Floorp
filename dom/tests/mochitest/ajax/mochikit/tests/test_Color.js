if (typeof(dojo) != 'undefined') { dojo.require('MochiKit.Color'); }
if (typeof(JSAN) != 'undefined') { JSAN.use('MochiKit.Color'); }
if (typeof(tests) == 'undefined') { tests = {}; }

tests.test_Color = function (t) {
    var approx = function (a, b, msg) {
        return t.is(a.toPrecision(4), b.toPrecision(4), msg);
    };
   
    t.is( Color.whiteColor().toHexString(), "#ffffff", "whiteColor has right hex" );
    t.is( Color.blackColor().toHexString(), "#000000", "blackColor has right hex" );
    t.is( Color.blueColor().toHexString(), "#0000ff", "blueColor has right hex" );
    t.is( Color.redColor().toHexString(), "#ff0000", "redColor has right hex" );
    t.is( Color.greenColor().toHexString(), "#00ff00", "greenColor has right hex" );
    t.is( compare(Color.whiteColor(), Color.whiteColor()), 0, "default colors compare right" );
    t.ok( Color.whiteColor() == Color.whiteColor(), "default colors are interned" );
    t.ok( Color.whiteColor().toRGBString(), "rgb(255,255,255)", "toRGBString white" );
    t.ok( Color.blueColor().toRGBString(), "rgb(0,0,255)", "toRGBString blue" );
    t.is( Color.fromRGB(190/255, 222/255, 173/255).toHexString(), "#bedead", "fromRGB works" );
    t.is( Color.fromRGB(226/255, 15.9/255, 182/255).toHexString(), "#e210b6", "fromRGB < 16 works" );
    t.is( Color.fromRGB({r:190/255,g:222/255,b:173/255}).toHexString(), "#bedead", "alt fromRGB works" );
    t.is( Color.fromHexString("#bedead").toHexString(), "#bedead", "round-trip hex" );
    t.is( Color.fromString("#bedead").toHexString(), "#bedead", "round-trip string(hex)" );
    t.is( Color.fromRGBString("rgb(190,222,173)").toHexString(), "#bedead", "round-trip rgb" );
    t.is( Color.fromString("rgb(190,222,173)").toHexString(), "#bedead", "round-trip rgb" );

    var hsl = Color.redColor().asHSL();
    approx( hsl.h, 0.0, "red hsl.h" );
    approx( hsl.s, 1.0, "red hsl.s" );
    approx( hsl.l, 0.5, "red hsl.l" );
    hsl = Color.fromRGB(0, 0, 0.5).asHSL();
    approx( hsl.h, 2/3, "darkblue hsl.h" );
    approx( hsl.s, 1.0, "darkblue hsl.s" );
    approx( hsl.l, 0.25, "darkblue hsl.l" );
    hsl = Color.fromString("#4169E1").asHSL();
    approx( hsl.h, (5/8), "4169e1 h");
    approx( hsl.s, (8/11), "4169e1 s");
    approx( hsl.l, (29/51), "4169e1 l");
    hsl = Color.fromString("#555544").asHSL();
    approx( hsl.h, (1/6), "555544 h" );
    approx( hsl.s, (1/9), "555544 s" );
    approx( hsl.l, (3/10), "555544 l" );
    hsl = Color.fromRGB(0.5, 1, 0.5).asHSL();
    approx( hsl.h, 1/3, "aqua hsl.h" );
    approx( hsl.s, 1.0, "aqua hsl.s" );
    approx( hsl.l, 0.75, "aqua hsl.l" );
    t.is(
        Color.fromHSL(hsl.h, hsl.s, hsl.l).toHexString(),
        Color.fromRGB(0.5, 1, 0.5).toHexString(),
        "fromHSL works with components"
    );
    t.is(
        Color.fromHSL(hsl).toHexString(),
        Color.fromRGB(0.5, 1, 0.5).toHexString(),
        "fromHSL alt form"
    );
    t.is(
        Color.fromString("hsl(120,100%,75%)").toHexString(),
        "#80ff80",
        "fromHSLString"
    );
    t.is( 
        Color.fromRGB(0.5, 1, 0.5).toHSLString(),
        "hsl(120,100.0%,75.00%)",
        "toHSLString"
    );
    t.is( Color.fromHSL(0, 0, 0).toHexString(), "#000000", "fromHSL to black" );
    hsl = Color.blackColor().asHSL();
    approx( hsl.h, 0.0, "black hsl.h" );
    approx( hsl.s, 0.0, "black hsl.s" );
    approx( hsl.l, 0.0, "black hsl.l" );
    hsl.h = 1.0;
    hsl = Color.blackColor().asHSL();
    approx( hsl.h, 0.0, "asHSL returns copy" );
    var rgb = Color.brownColor().asRGB();
    approx( rgb.r, 153/255, "brown rgb.r" );
    approx( rgb.g, 102/255, "brown rgb.g" );
    approx( rgb.b, 51/255, "brown rgb.b" );
    rgb.r = 0;
    rgb = Color.brownColor().asRGB();
    approx( rgb.r, 153/255, "asRGB returns copy" );

    t.is( Color.fromName("aqua").toHexString(), "#00ffff", "aqua fromName" );
    t.is( Color.fromString("aqua").toHexString(), "#00ffff", "aqua fromString" );
    t.is( Color.fromName("transparent"), Color.transparentColor(), "transparent fromName" );
    t.is( Color.fromString("transparent"), Color.transparentColor(), "transparent fromString" );
    t.is( Color.transparentColor().toRGBString(), "rgba(0,0,0,0)", "transparent toRGBString" );
    t.is( Color.fromRGBString("rgba( 0, 255, 255, 50%)").asRGB().a, 0.5, "rgba parsing alpha correctly" );
    t.is( Color.fromRGBString("rgba( 0, 255, 255, 50%)").toRGBString(), "rgba(0,255,255,0.5)", "rgba output correctly" );
    t.is( Color.fromRGBString("rgba( 0, 255, 255, 1)").toHexString(), "#00ffff", "fromRGBString with spaces and alpha" );
    t.is( Color.fromRGBString("rgb( 0, 255, 255)").toHexString(), "#00ffff", "fromRGBString with spaces" );
    t.is( Color.fromRGBString("rgb( 0, 100%, 255)").toHexString(), "#00ffff", "fromRGBString with percents" );
    
    var hsv = Color.redColor().asHSV();
    approx( hsv.h, 0.0, "red hsv.h" );
    approx( hsv.s, 1.0, "red hsv.s" );
    approx( hsv.v, 1.0, "red hsv.v" );
    t.is( Color.fromHSV(hsv).toHexString(), Color.redColor().toHexString(), "red hexstring" );
    hsv = Color.fromRGB(0, 0, 0.5).asHSV();
    approx( hsv.h, 2/3, "darkblue hsv.h" );
    approx( hsv.s, 1.0, "darkblue hsv.s" );
    approx( hsv.v, 0.5, "darkblue hsv.v" );
    t.is( Color.fromHSV(hsv).toHexString(), Color.fromRGB(0, 0, 0.5).toHexString(), "darkblue hexstring" );
    hsv = Color.fromString("#4169E1").asHSV();
    approx( hsv.h, 5/8, "4169e1 h");
    approx( hsv.s, 32/45, "4169e1 s");
    approx( hsv.v, 15/17, "4169e1 l");
    t.is( Color.fromHSV(hsv).toHexString(), "#4169e1", "4169e1 hexstring" );
    hsv = Color.fromString("#555544").asHSV();
    approx( hsv.h, 1/6, "555544 h" );
    approx( hsv.s, 1/5, "555544 s" );
    approx( hsv.v, 1/3, "555544 l" );
    t.is( Color.fromHSV(hsv).toHexString(), "#555544", "555544 hexstring" );
    hsv = Color.fromRGB(0.5, 1, 0.5).asHSV();
    approx( hsv.h, 1/3, "aqua hsv.h" );
    approx( hsv.s, 0.5, "aqua hsv.s" );
    approx( hsv.v, 1, "aqua hsv.v" );
    t.is(
        Color.fromHSV(hsv.h, hsv.s, hsv.v).toHexString(),
        Color.fromRGB(0.5, 1, 0.5).toHexString(),
        "fromHSV works with components"
    );
    t.is(
        Color.fromHSV(hsv).toHexString(),
        Color.fromRGB(0.5, 1, 0.5).toHexString(),
        "fromHSV alt form"
    );
    hsv = Color.fromRGB(1, 1, 1).asHSV()
    approx( hsv.h, 0, 'white hsv.h' );
    approx( hsv.s, 0, 'white hsv.s' );
    approx( hsv.v, 1, 'white hsv.v' );
    t.is(
        Color.fromHSV(0, 0, 1).toHexString(),
        '#ffffff',
        'HSV saturation'
    );
};
