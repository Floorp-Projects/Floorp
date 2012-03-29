try {
    ({
        f: evalcx("evalcx(\"e\",newGlobal('new-compartment'))",
                  newGlobal('new-compartment'))
    })
} catch (e) {}
gc()
gc()
