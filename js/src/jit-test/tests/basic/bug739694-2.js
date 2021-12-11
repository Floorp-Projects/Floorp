try {
    ({
        f: evalcx("evalcx(\"e\",newGlobal())",
                  newGlobal())
    })
} catch (e) {}
gc()
gc()
