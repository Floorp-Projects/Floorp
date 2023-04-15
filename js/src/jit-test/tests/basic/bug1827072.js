try { newString("", { capacity: 1 }); } catch (e) { };
newString("x", { capacity: 2, tenured: true });
