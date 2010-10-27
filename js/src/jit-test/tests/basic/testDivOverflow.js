for (d in [0, 0]) {
    const a = (d -= (++d).toString())
	for each(b in [Number(1) << d, 0, 0xC]) {
        b / a
	    }
}
