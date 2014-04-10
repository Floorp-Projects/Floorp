// These tests just mustn't trigger asserts.
Object.prototype.get = 5;
new Intl.Collator().resolvedOptions();

Intl.DateTimeFormat.supportedLocalesOf('en');
