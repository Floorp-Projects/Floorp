// |jit-test| --more-compartments;
fullcompartmentchecks(true);
newGlobal().eval(`import("javascript:")`).catch(() => {});
