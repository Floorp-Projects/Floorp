function hasPipeline() {
    try {
        Function('a |> a');
    } catch (e) {
        return false;
    }

    return true;
}
