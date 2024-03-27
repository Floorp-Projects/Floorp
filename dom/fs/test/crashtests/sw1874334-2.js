(async () => {
    let arr = new ArrayBuffer(43109)
    let blob = new Blob([arr, arr, arr, arr, arr])
    let req = new Request("missing", {"headers": []})
    let dir = await self.navigator.storage.getDirectory()
    let file = new File([blob, arr], "", { })
    let handle = await dir.getFileHandle("514600c6-596b-4676-ab0c-3e6f1e86759f", {"create": true})
    let wfs = await handle.createWritable({"keepExistingData": true})
    await handle.createWritable({"keepExistingData": true})
    try { await req.json(arr, {"headers": []}) } catch (e) {}
    try { await file.stream().pipeTo(wfs, { }) } catch (e) {}
})()
