try {
	[0,0].sort(Array.some)
	"".replace(RegExp(), Array.reduce)
} catch (error) {
	if (!(error instanceof TypeError && error.message == "0 is not a function"))
		throw error;
}