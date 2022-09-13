"use strict";

// TODO verificar se o navegador pode deletar objectstorage especificos

const translationCache = {};

translationCache.google = {}
translationCache.yandex = {}
translationCache.bing = {}

{
	function getTableSize(db, dbName) {
		return new Promise((resolve, reject) => {
			if (db == null) {
				return reject()
			}
			let size = 0
			const transaction = db.transaction([ dbName ]).objectStore(dbName).openCursor()
			
			transaction.onsuccess = event => {
				const cursor = event.target.result
				if (cursor) {
					const storedObject = cursor.value
					const json = JSON.stringify(storedObject)
					size += json.length
					cursor.continue()
				} else {
					resolve(size)
				}
			}
			transaction.onerror = err => reject("error in " + dbName + ": " + err)
		})
	}
	
	function getDatabaseSize(dbName) {
		return new Promise((resolve, reject) => {
			const request = indexedDB.open(dbName)
			request.onerror = event => console.error(event)
			request.onsuccess = event => {
				const db = event.target.result;
				const tableNames = [ ...db.objectStoreNames ];
				((tableNames, db) => {
					const tableSizeGetters = tableNames.reduce((acc, tableName) => {
						acc.push(getTableSize(db, tableName))
						return acc
					}, [])
					
					Promise.all(tableSizeGetters).then(sizes => {
						const total = sizes.reduce((acc, val) => acc + val, 0)
						resolve(total)
					}).catch(e => {
						console.error(e)
						reject()
					})
				})(tableNames, db);
			}
		})
	}
	
	function humanReadableSize(bytes) {
		const thresh = 1024
		if (Math.abs(bytes) < thresh) {
			return bytes + ' B'
		}
		const units = [ 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB' ]
		let u = -1
		do {
			bytes /= thresh
			++u
		} while (Math.abs(bytes) >= thresh && u < units.length - 1)
		return bytes.toFixed(1) + ' ' + units[u]
	}
	
	async function stringToSHA1String(message) {
		const msgUint8 = new TextEncoder().encode(message); // encode as (utf-8) Uint8Array
		const hashBuffer = await crypto.subtle.digest('SHA-1', msgUint8); // hash the message
		const hashArray = Array.from(new Uint8Array(hashBuffer)); // convert buffer to byte array
		return hashArray.map(b => b.toString(16).padStart(2, '0')).join(''); // convert bytes to hex string
	}
	
	const cache = {}
	cache.google = {}
	cache.yandex = {}
	cache.bing = {}
	
	const db = {}
	db.google = null
	db.yandex = null
	db.bing = null
	
	function getCache(translationService) {
		switch (translationService) {
			case "yandex":
				return cache.yandex
			case "bing":
				return cache.bing
			case "google":
				return cache.google
		}
	}
	
	function getDB(translationService) {
		switch (translationService) {
			case "yandex":
				return db.yandex
			case "bing":
				return db.bing
			case "google":
				return db.google
		}
	}
	
	function useDB(name, db_) {
		switch (name) {
			case "googleCache":
				db.google = db_
				break
			case "yandexCache":
				db.yandex = db_
				break
			case "bingCache":
				db.bing = db_
				break;
		}
	}
	
	function openIndexeddb(name, version) {
		const request = indexedDB.open(name, version)
		
		request.onsuccess = function (event) {
			console.info(event)
			
			useDB(name, this.result)
		}
		
		request.onerror = event => console.error("Error opening the database, switching to non-database mode", event)
		
		request.onupgradeneeded = function () {
			const db = this.result
			
			for (const langCode in twpLang.languages["en"]) {
				db.createObjectStore(langCode, {
					keyPath: "key"
				})
			}
		}
		
		return request
	}
	
	function queryInDB(db, objectName, keyPath) {
		return new Promise((resolve, reject) => {
			if (!db) {
				return reject()
			}
			
			const objectStore = db.transaction([ objectName ], "readonly").objectStore(objectName)
			const request = objectStore.get(keyPath)
			
			request.onerror = event => {
				console.error(event)
				reject(event)
			}
			
			request.onsuccess = () => {
				const result = request.result
				resolve(result)
			}
		})
	}
	
	function addInDb(db, objectName, data) {
		return new Promise((resolve, reject) => {
			if (!db) {
				return reject()
			}
			
			const objectStore = db.transaction([ objectName ], "readwrite").objectStore(objectName)
			const request = objectStore.add(data)
			
			request.onerror = event => {
				console.error(event)
				reject(event)
			}
			
			request.onsuccess = function (event) {
				resolve(this.result)
			}
		})
	}
	
	translationCache.get = async (translationService, source, targetLanguage) => {
		const cache = getCache(translationService)
		let translations = cache[targetLanguage]
		
		if (translations && translations.has(source)) {
			return translations.get(source)
		} else {
			cache[targetLanguage] = new Map()
			translations = cache[targetLanguage]
		}
		
		const db = getDB(translationService)
		if (db) {
			try {
				const transInfo = await queryInDB(db, targetLanguage, await stringToSHA1String(source))
				if (transInfo) {
					translations.set(source, transInfo.translated)
					return transInfo.translated
				}
				//TODO RETURN AQUI DA LENTIDAO
			} catch(e) {
				console.error(e)
			}
		}
	}
	
	translationCache.google.get = (source, targetLanguage) => translationCache.get("google", source, targetLanguage)
	
	translationCache.yandex.get = (source, targetLanguage) => translationCache.get("yandex", source, targetLanguage)
	
	translationCache.bing.get = (source, targetLanguage) => translationCache.get("bing", source, targetLanguage)
	
	translationCache.set = async (translationService, source, translated, targetLanguage) => {
		const cache = getCache(translationService)
		if (!cache) return false
		
		if (cache[targetLanguage]) {
			cache[targetLanguage].set(source, translated)
		} else {
			let translations = new Map();
			translations.set(source, translated)
			cache[targetLanguage] = translations
		}
		
		const db = getDB(translationService)
		
		if (db) {
			try {
				addInDb(db, targetLanguage, {
					key: await stringToSHA1String(source),
					source,
					translated
				})
			} catch(e) {
				console.error(e)
			}
		}
		
		return true
	}
	
	translationCache.google.set = (source, translated, targetLanguage) => translationCache.set("google", source, translated, targetLanguage)
	
	translationCache.yandex.set = (source, translated, targetLanguage) => translationCache.set("yandex", source, translated, targetLanguage)
	
	translationCache.bing.set = (source, translated, targetLanguage) => translationCache.set("bing", source, translated, targetLanguage)
	
	openIndexeddb("googleCache", 1)
	openIndexeddb("yandexCache", 1)
	openIndexeddb("bingCache", 1)
	
	
	function deleteDatabase(name) {
		return new Promise(resolve => {
			const DBDeleteRequest = indexedDB.deleteDatabase(name)
			
			DBDeleteRequest.onerror = () => {
				console.warn("Error deleting database.")
				resolve()
			}
			
			DBDeleteRequest.onsuccess = () => {
				console.info("Database deleted successfully")
				resolve()
			}
		})
	}
	
	translationCache.deleteTranslationCache = (reload = false) => {
		if (db.google) {
			db.google.close();
			db.google = null
		}
		if (db.yandex) {
			db.yandex.close();
			db.yandex = null
		}
		if (db.bing) {
			db.bing.close();
			db.bing = null
		}
		
		Promise.all([
			deleteDatabase("googleCache"),
			deleteDatabase("yandexCache"),
			deleteDatabase("bingCache")
		]).finally(() => {
			if (reload) {
				chrome.runtime.reload()
			} else {
				openIndexeddb("googleCache", 1)
				openIndexeddb("yandexCache", 1)
				openIndexeddb("bingCache", 1)
			}
		})
	}
	
	
	let promiseCalculatingStorage = null;
	chrome.runtime.onMessage.addListener((request, sender, sendResponse) => {
		if (request.action === "getCacheSize") {
			if (!promiseCalculatingStorage) {
				promiseCalculatingStorage = Promise.all([ getDatabaseSize("googleCache"), getDatabaseSize("yandexCache"), getDatabaseSize("bingCache") ])
			}
			
			promiseCalculatingStorage.then(results => {
				promiseCalculatingStorage = null
				sendResponse(humanReadableSize(results.reduce((total, num) => total + num)))
			}).catch(() => {
				promiseCalculatingStorage = null
				sendResponse(humanReadableSize(0))
			})
			return true
		} else if (request.action === "deleteTranslationCache") {
			translationCache.deleteTranslationCache(request.reload)
		}
	})
}
