// |jit-test| error:Error

if (this.Intl) {
    addIntlExtras(Intl);
    addIntlExtras(Intl);
}
