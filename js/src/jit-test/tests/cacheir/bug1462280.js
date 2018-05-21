for (var i = 0; i < 2; i++) {
    evaluate("var obj = {[Symbol.iterator]: Symbol.iterator};");
}
