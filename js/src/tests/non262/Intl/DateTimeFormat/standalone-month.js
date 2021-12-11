// |reftest| skip-if(!this.hasOwnProperty("Intl"))

for (let weekday of ["long", "short", "narrow"]) {
    let dtf = new Intl.DateTimeFormat("en", {weekday});
    let options = dtf.resolvedOptions();

    assertEq(options.weekday, weekday);
}

if (typeof reportCompare === "function")
    reportCompare(0, 0, "ok");
