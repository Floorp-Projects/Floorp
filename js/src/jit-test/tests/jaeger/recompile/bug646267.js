function t(code) {
    var f = new Function(code);
    try { f(); } catch (e) { }
}
t("");
t("");
t("");
t("this.function::a = 7;");
