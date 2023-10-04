async function timeout (cmd) {
  const timer = new Promise((resolve, reject) => {
    const id = setTimeout(() => {
      clearTimeout(id)
      reject(new Error('Promise timed out!'))
    }, 750)
  })
  return Promise.race([cmd, timer])
}

(async () => {
  const root = await navigator.storage.getDirectory()
  const blob = new Blob(['A'])
  const sub = await root.getDirectoryHandle('a', { 'create': true })
  const file = await root.getFileHandle('b', { 'create': true })
  await file.move(sub)
  const stream = await file.createWritable({})
  await stream.write(blob)
  const sub2 = await root.getDirectoryHandle('a', {})
  await sub2.move(root, 'X')
})()
