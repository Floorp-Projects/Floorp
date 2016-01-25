try {
	[0,0].sort(Array.some)
	"".replace(RegExp(), Array.reduce)
} catch (error) {
	if (!(error instanceof TypeError && /^\w is not a function$/.test(error.message)))
		throw error;
}
