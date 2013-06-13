/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const SEARCH_ENGINES = {
  "Google": {
    // This is the "2x" image designed for OS X retina resolution, Windows at 192dpi, etc.;
    // it will be scaled down as necessary on lower-dpi displays.
    image: "data:image/png;base64," +
           "iVBORw0KGgoAAAANSUhEUgAAAIwAAAA4CAYAAAAvmxBdAAAAGXRFWHRTb2Z0d2FyZQBBZG9iZSBJ" +
           "bWFnZVJlYWR5ccllPAAAGrFJREFUeNrtfHt4VdW172+utZOASLJ5+BaIFrUeXkFsa0Fl++gDnznV" +
           "VlvFxt7aqvUUarXtse3Bau35ak/rZ9XT26NtfOvV6wFET+FYCQEKWqsQIT5RCAgSXnlnrzXneNw/" +
           "1lphJSSQ8BB7bub3zW+LO3uN+fiNMcf4jTEX0N/6W3/rb/2tv30smtnXB3zmRi2FQakxQNKX3WkW" +
           "9S/tgW3HLpmQM543A0BWVSHMYGIwOTDxzxrOf3/RQQfMZ2/SLAvKhTFVBGUqKFONH2QAzwOMF38a" +
           "wHhYZAxWAqhe/iszp3+b970d/sInc57vz/J8L2eMB2MAEYkBQ6DQ3dRw4dq7AUjcP3rAfPZmLWXC" +
           "LHKoIAcQAUxaB5EaEfc6AEBhjDEwmcx43/fO9HxT4vkReBIAAZgjgodW3NcPnn1sHgD/iHknn+0d" +
           "6s8XEUhsXXac/34WAAGw8afuT8GZ3X055YeSJcIsG+pMZwFn0UihezRofPt3G54f/0E8cNMN+Myo" +
           "8jVTCgYd823PLzrPeIBnABiUQ1F+UoWsVOYb33mkoKp/7/dKyT0AGc47X4s0sjBEoLxbBqAQAMfW" +
           "Rfe38B4BM+VHUkYOs8mi1FrABbK4dcvK73zwp1M3xYPOxANKBqbpCdXNGb0UwPKRF74xpfDQ0t+K" +
           "54+IvlKoahmAhaO/mv/ZmicG3tqPgT61ZM2dZMQJOYhIdByRM/F3dCCOox4Bc3oEliqyyNoQCPPu" +
           "sXceKZqRsigu7pwaWBowiRb46+f9Q1V2wl1nDx09/R7jF30x9adNlN8yPx4DHwht+B/cBIBoRqeI" +
           "E4hE/oshTcB0wNbT6/o/zrhFyohR5ZxmrVWE+fDxdx4puhGAH4OkPe5B6pykeJAc/7cDEMZ/095Y" +
           "870P339m+BXs2v4kbCFsm9u2vnpJ3bzR7wAo2B/R2v+PjSnyXcRxtOLUSXFxwAFz5i2SZUIVO82S" +
           "BWye/vLOIwNvjL8OYqCEfXCmJAZPHkC7sK1REbj2+lmbq86qTVmmfuuyN2cTiREWKCvACgml9kDL" +
           "7HQksehsZmSdA6yVpsa6P38v3swg7m4vN1dGXrThKGP8yS5fP33j/LEvxKDbl2f2A0YFCtkZQDOa" +
           "PjLAnP4jrmBGjh1AVhG2ttxfX33++vjY2eeNXf/siLUAzgEwMJZrY2vF/Vu/t4BRqCqgCmj07wMV" +
           "HXUCzJQfUlZE72ICnANcqNj21h8eiK1AX46gXh29KT9H+rd9XxBjYGCgig7QHOgjPgMAKigXQZYp" +
           "si4uCOc3v35zY2wF9ufGSgxA7fdd9g8ho9ol4P4ojiQWnSUMMANECrJNy1NWYH8eGfsEvJbLv1IK" +
           "1XIAUwEtA0xplJMwjcaYlTDeShg8dOgjj6/cJxNYfWIWkHJoh5yyjkSZ8RbB89YBZq4/pXafGeuz" +
           "b9WciXJxo2B2houqgAjABJCLOwFMqFv57+bBxMIAJm1det3avnl1OYCLAeSgWhofaY1QXQSRuYc+" +
           "/OiD3QLmUzNdqTBKhRVMADsF5beuToXJB90KtFz+lVIVniXOVUAUqjpXVB4WwPjGTPB8/0zjeTnj" +
           "ezl43szmKy6vNkDF4MeeXNc3oJyUhfAMkJsJkSxUVrLos6o6z/O8Ucb3phrPzyHKeVTwkpPXseg3" +
           "Cqe+1SfG+swfaw6KGTAoJ5eyGF3IBeEIJB2AcXxb0FI/L45uFQBMGiu6Z3ai9eqrclBUClFWVatV" +
           "5GERNT5wEVQnQLUcIuVNX75kFjn60rA5c1d0AoywlkcxfdwZ2LSgbOmBZAv70povu7RcyFUqcZYd" +
           "Pbxix44fnLv8pbYUOWh+P3ZM9uJRo34xoLDgq8b3YTxvqhqsaPzyJTdmn36msjdyqPqkMhWqBFGZ" +
           "MtV8uDX4zMjp2zemyEoPgGn4zyOvGzy48A54GcD3Sz1jFrqqE+4uOOvdmb0ASlYEs5mQE9afUdhy" +
           "0yv3lHzwya/8ZcjgI0+5yssU3QKYkgQ4Ivp60LL1n8kBQfOWuvdnj6uLldgHQKoKxU7HV/eg2y1X" +
           "XXmXEs1U0ZVb29o//4k5c5P5eQB+s+68aVeUFBTcCxUoS6kRWfjhueecc9SfX3ytA9QTr7eVACqY" +
           "FDYEwnbB2qcHHg6gLY6ODhpomi77coUyVaojhKH9+ZHzF/wqXiztEg34APxNX/jCvQOLCi83fpy8" +
           "UsCJXHLYnGdn785S0uKTyyBUBXJZcW5x4bSN56ciyLQcD4Bf/+ThVwwbUvRb+JkoswqAWX5b9Lm1" +
           "M3uSM/UnUiaCKiZk2blvvnxX0ePxuBNAmpMur51wyLBPzjVeBBoVwIXBk6vuP+SG+LkcuwkWAA96" +
           "/JjZKnKxkACkkFb5Nztz220xX9bJlWi+6opKFalQlpqlmzZNu6B6SaJ0knKJ/DW5qd8p8TO3x6AB" +
           "qza1EE06cdmy9wDAY5LjmBTMkQnUnZ42H0ywNF52aU6FK4UY5NySI+cv+E3MCnMM5HyqtwFoO3rB" +
           "gmuDMFjGjiCOIEQwzH9c+7lzju+JTaYlJ2ehUqXMWWFqeurFxqsAFMVf25Ss9kTOEZdvebClJbxT" +
           "yUGZoEzwlL/b9tzRX+pOztSfSBZApSqyIrL45buKnkaUJEzLCN5+csxr+ab6fyILkI2OIZYBlx9/" +
           "2bYvpLgw2+EqKLKdwoceVKJp+tfuEpYKZcaW1tZbLqheEsbj3GV+oxdV3x0GwQZrHUIiWKIST3Vm" +
           "DG54zFrKrBBWiGgSyx9Uv6Xh0n/MKlGlOII4h80trQ+kuJt8HGklZHg6FZF/Y/uOb7O1YOvAzkGt" +
           "Kxmoehe6SYNEpkErwZIFC4I2fuLKf2tLtDOPzumPhA6wAPJDLt1yuzjaAEcAMUCMApXfvPP7IcO6" +
           "gkYFs4RRpgy49qanUsAPu/T8W48e/YwL6S/kYtBYwM8U/yu6KVlQUShr9CkKyK7b1vDVy0qVeaYy" +
           "gaxbdeK85/8a/z7sYR3zgXM1gXUInEPoCEw8PR6z8YQxaidQPh6RrgrPEOZS4chKjFuydEEKFD1x" +
           "QgrAnfO3V98Jw/B5dhFgmByU+MK/nnrq6K6gcQtPyqlIubJAibCxPv/fsVVNgCI9yGEAQdBq71NH" +
           "UEdQIoBo5PBBeklazuQfSpYFM0UAFsDmd2yMf9+1XkUT3otc8AiRwpFChCBCI0detGbSLtYr5uw6" +
           "tk26XctZwgxhRt65ZSmr1t389M1Jk85wzKcHRAiJkCfasDnI/0sMGN+jlLMrAigMhp0+f+TBBIw4" +
           "milEYOcQBHZZAoZeEIgKgIIgeJbD2MqEFhxaDAFmdAWMisxQFigzlAUnX9e4rA9yeHuTna3koBQB" +
           "RogxwOPvxNbQAAA7VHQEFKSQKEFIu4lA5d3HiiuFNB4XQZlhUHBK11QO0oRdD7ouROVCkeJZG7ak" +
           "/KBOYHlz4sTy1WVlVY5oYego2+bs82+3tFw6YcVrp01dteqpxNfyhKQuGlxCMSsKBh570ABT/8XP" +
           "5dhRVpyDWAd2Ns0O9yrhWdfcMpvCEByEoNCCwhBgvgBdM+PM5TH5FPW+1ZLo8de2viehe12dhVoH" +
           "OAtDPO61O4o+kYCTnE5wVuGsxlzKHul7BUDKdomKgwpB2QHAyNiP2Dl+0Z2WRXZ9YP0F55WJczvX" +
           "0jp09U3fLiurWD1+/NqQaHZIVNbu3O1vt7aM+fSqVRWXvPvu0pRldwAkQ5brjO+NMh0kgMIvGjYZ" +
           "wIKETPxIrYt1U5M8iThKJil9yZGc++ab298dP36Jb8wZohqhQHRErKEeAA6fG5FT5yIlYYI6tzfO" +
           "vtiQni3MYDw0ChqEgUMyejyAdwGwDeW4ZI9FAGQOmwzgv/cERmZbDXhnKBNUGMJkUhGVduSSJJ1P" +
           "6rw8HIalJo7ilBkchgCgL48fVzLceDc4kZnWUdap1AQi10x+660n4jXyk1M7ZXEZgHhMUkMO4Njp" +
           "hQGMf8h56Fx++ZE1a+1xZC2Szjs3sk9uUEhUbSMvP3LeyOGZ0tKJiearo1J1DHVRPYmS7JUcG2g1" +
           "pxxUsooBnpmQWAOb10YbKGygcKFCZOC0XqxrRKokCBQG5euX77In2k1P+2hhWEZBAAoCuCCEcW7E" +
           "2xMn/m6oYo0jyjnmuc3Off6UN96YMvmtt5LILSmQ61r3xAA0I+xqPBiIejAd1f7e2MPPfvm4LQs/" +
           "89a+bP6nZuSzfsaU+T7g+UBixYQVRFGS01kFO22srRy0EgA4CEvFRHS3MANMY/fGbybmlQqAFSBV" +
           "sCp8kWwCGA5dqefFShnnRV77ecHYU37iXuqLoB0tsuIo34v3NfJR1GlJsrnOuiXGy1y8k+rwxh57" +
           "3srSD/6rbLdra7yMqgjUCGAULR8uWr0LJPYAGApCeCbKNygLPKIxJ65YOSU+YpLUUCYGiqBzQVy3" +
           "Ft1zbevnJl60UARqACgcVDo9ZZr63Mqua68QxlpmrWJC1FmrmLSKCFVktcpZrbKhzg4D26E5Lgjg" +
           "8vnoMwwh1hU/dvTRo/qcDyJqcESw5Dp6o3XNHVrqLDSubAdFjuXwwWZcX+Wc9APboKxQUoiLurXa" +
           "IYfCpjlCDsoxZ6OCouLRt+xpbY3nA8aDMR6E2+9vffOWxl02cQ+Bbdjevt7l83D5ABRaKNHYO484" +
           "YmgMkoJ4jElCOL8Lz9NN87YumrRDxc2DElQZKgIVhZcZcO1hZ74wtK/H0thvtuXGXdM2S0S/ziQ1" +
           "FPJiG7pHwvbgDhtKnQ0VNhCEeUHQLmiuf2fymieGvJGY8DCfX+yCEC5xWIlwtO+P6+s4VESJGS4+" +
           "liwxKjZ/2FGRZvPhYgktxEZdHWOAr2P34ihWIQWTgJ2CnWJbo9Ymz1g/5+h1QsF9wgKJ19Z4hV87" +
           "4fKNE3cnx8v4V8H4UOjqhvce+zW6qdWVlOvSjQsDlw/WUT4A5QNQGIJDizMPHXR+CiRBb4GSzlYr" +
           "26Z7vYKSC42nUOPBqA9VU1I0ZOJPEYWj1NvVW/3AoEUAFgO4IzZ1hYk2jf9WUw7IjCIXHUVhXrFp" +
           "/sQtKZPIoXXr/PjoSkZeoHo6gP/bFyeciECqcHG3IrXp37a2SF3xQNPxRAXgq5nS1bHsDWCYALYA" +
           "u+h0W/impI8Pad9ec/vAoWVTjV84Nsn5FAwcvmDMN5rOqf1jyatdHzjuGjvThloKYH3b5qVXt775" +
           "44ZuN1QEKknF3a6ImfDee4tWjBrV6R5Qoeq1AP6Avaxx8gDolhdPXAh2qzQmZFQ4ZhALrj/mvLpT" +
           "+qhxya0BP5VVZQBkA6jNR0AJ2xUUcjKGjsx4k3PVYUwaJU6rJ3reLiHlHppjBjF3fLYSzU/noEZ8" +
           "3611VusoVJBVsFWAdezim/3jemSFe+SNIsvCpAhCXf7TBZI+PnTr4nO2t2xcME3ZroYKIouEEqDo" +
           "xfHfav/GxOttFgBOucGWll0XVqrqXYDWNLz3aG7bsovWp4i2TvkhScLqNBezq/M/zxLBxV2Yx/75" +
           "yCPP6usc04CJ+B3bcLMwQTiK+0UIwgz1ip8+4pyaYX0x0SnWMkjnYGygkm9nBO0MGzoI2TTDyQBw" +
           "7ubNawPmeZYZNt5wZhrxX8OHX9yXSTJzGcVgIWasbs8/hc7XRzXM670cg0Vs5H+MHm6u74ucrb/K" +
           "lAlFPoySoqFFn+rm+OCGV762df2cYWe4fP0M5qDWhoowRIm1/h+s1YZx3wrVOV1LDhXMaGzfXntF" +
           "46vXtMQRS/clsqRRT9SNd0GMBo6edRStZbKeg4D//ciQIcP2CTDbqsdVKQePq1JMFkXxv4qO9AaM" +
           "fPGoaeuG9kXp0LkU0wGgMFC1gYAdAeyg0m3IrE3W3mtTvodjRpHq9X3xL4h5Qsq63P/z9ra6LqSc" +
           "vvmBPkwOTex2lnf4wNee/47fa99NGGVJ8Zl1qP3UPfwkdr15mDDV+Y3Pf+Kh9c9kz9pee89J7dve" +
           "vaRt+7qLbVv47y5UUKggp3BB/okNz0/aHI8332OaIgELxWDpptQtt6X+Qcu03nVYGQYxjxzl+7/e" +
           "GyvjdYrCtv31JiW7QTjy6qWj83jF4AeP/MLaodiHRtZBXAihEEIWkq4eSgGmvKGhqpX5d1YEVhiW" +
           "BaI6Zf6QITN7s5ELhw4tZZavkwhIZMOC1rZfo5s64nPv4+1NzXot2/hYiqKckglH4/7eRojCOosp" +
           "St6u2ijfS1Hv3I0SdVy5aam9ecumBeOqN8w7aRkxSlMVdRDmRHa4m5xWPKPEusUA6maIrcy/cCKw" +
           "InASKaCoXrlo2LAH+xpMpAEjLauu2ObaNnxVmZqUHaI8SaR+KnIhTPHCo6ZtOn6vk4qUPNNGnV2P" +
           "J0ptENweMq92zHBMcMwwIrfMLS6etKdJEnMlCYOZm9YE4dUPkWvsIUckJ/+SZwd5PCEOEBc5rh7j" +
           "grqf+VfvSc7mO/xZSihVAra3YMY/PqqrUhZVe7C8yRHTBqAVQJuQN5idgJ2ASQAz4PJjptWevKc0" +
           "RZQ0TQATRWDd/dmFDQ2VeaLH0z4dRVTK9EXZ7IqFJSXH7W6eLw0blntp2NAydGOSqPGVs/5mW9Zc" +
           "JGKbRSxELIRDCFuIuAmiBa8eMW37rcdc1JDtM+3PYdSp43k9/ulPgmDrsnz+vFBktRWBZYEVKSlU" +
           "feH5wYPP7u5Hfy4uzi4oLq50IjkSaXrf2vIfBPnV6PlKiwKg0XfyNe2BPkmJ8+oUGeh/bLjNu7En" +
           "0Gy+w5sppLcyKRra9IZJ98hTvciop9MPSSFUwGTnEjHICsgpyKHYHzjquWMvrJ+wewUENPFjCIAx" +
           "k3uStyIMbw5FVieWJvJpBE5kgqq+X1VcPGdRcfHMxSUluSUlJbmlUZ+1tKRkLRGVnrZ9Rw12rSLt" +
           "sDpFg8vmfbpw0HH3wcuMMSaiao2XAbwMjPFhPL/ReN6DfsY8tHHekN0WXR929vqsCpWruFshPEqF" +
           "o3IyADuWTxgea1rYTbRVeEMmc+SnCwp+OcB4l3kmLq0D4BnzkA/MMUBjvDMXC1DBqlkCFr9N9E//" +
           "HIZpPyDsQVuTFwsMfP273k8GFeLbvo9izwe8DGA8VMPgIc/D2piALlPFDGWUMqNuazOun/RbeQU7" +
           "L/zl0cfC+SPOXjG84NBRawCvJNoSE7PiBgr5Xx/MKf7jLnzIbUPKlHVF5C11KgJfD9+shY8Vxjd3" +
           "0780rEvP8bFDDvnVQGO+lU5MeTDwzM5aTbOzNyrw/XNbWx9JFLknk+sjqjobUHJq9XS/cNj3jZcZ" +
           "Ac9PwBIDyAeMD2O8RhhvpTFYqYpGqMQOM2UhlFOhsvjfgNJ6ofxyoZaXbHPt8mDNjDU9ACYBbyGA" +
           "AT/KZEZ/MpO5qciYyRlgROeJGSh0nQCL21Ufmx4EL8dMpqScRt4DFVAAYMCtORx+0Rhz7aFF+GJB" +
           "BmNM/JKklGo1KlBtHZ474U79P9hZOZcQYb0unD/mwu05qADCZwE4C8Y7I3kTk4kFx+mUuzfMKf5e" +
           "+rn+rUMq4PR4hFII0gw0xpdvGAWGoDqHf9m8IuV8m2Qtf1pQMPok37+50JhpHlC8EzwRcAzwOqs+" +
           "Vkv06I+da04nInd3RvuxgCIAhcUTF5zvFQ79oucP+Cy8zIjE6qQnt5Pviu5IqAogVKNCNSrBUte6" +
           "blnrqi/Vo3O9rI3Pc7cbP6sgGQcAf7rvl3zK908uBKjAGK5jrrmNKKHj/RS3E6L3V2USLUzkZAB4" +
           "i75pTivwwQMyoKYQ685+QOtScvzUHPbIlJ54ZVsuDPTrZDmnQqUQggo1qkoNRDyFeJ6XGQfjF0fW" +
           "3O9YWxW6adNzw36Dzm/JKEJ0k7QgtfiSygd1vSrkdZ3jlb6fneT7Y+MN1xrmVX9gbkw9q1MdsemF" +
           "U5wkpwqSRSw49gfZAcPPHOsVlIww/sBjjPEVnqfGZEQlWKVCjWK31TW/dv56pCruU126TGxPl+US" +
           "IrAgNQ7TQ+pNukQqfalLNimApvMt6CZMTvsiu3VOJ17XnrNWZ9m85oK8Qmz4sFB+CeXrF29dfOqG" +
           "1PwKs6fOKyvKjrnb8wrHGD8TWfCOEoX85zb96dgXY9leN2NM+y3SJZG4u7XsSldIykFPz09NHxbR" +
           "T2U3M11AsKf8aRqtnBqQoG91oWkGOS0/XaQo2Pf3u5mUDK9LukD7Mv5Tv9teSQ4VzipsINUtW9Zc" +
           "t/mFiRu7WbcOuQNP+MXQ4hGX3mEKBl1mjB9bbwAqSz6cf+TZ8Qaabta/u6hM92ItpZs5dvyor5R/" +
           "dwvp9QAa6eFzfxRlpVMk2mXh93czeyPn1Bn5ShWtYAJsyEve+OPgC7Hzmgx3USDtejQedlbtDX7h" +
           "0Ns6HChV5LcvP7rpb1+qx/690dHrtewL05c2c7ZLtrM91fOpDGjXyvT9+WYBPQAg3NPcey1n4vVt" +
           "FUJSIfGNjJZNy2ekkqzpazIJOefSoTaA9q1VY+5Wbvs9NAoYVBkFh5Sesi9lJ/u6lt5+WETpoi2M" +
           "PpZU/k9szmKGtVGRWBjQ6g3zP78pxfSGKb+tJ4LPAsi31S/+uXCUlVZmCIc+DlI15L4Cpr/1FA1d" +
           "0VLqAilzgcCGChdQc5eoTXqpkNS66hv1YLsUElURiG1sOZj7lunf3v3fwlBKjRfX9EjEHKcscV98" +
           "D40zRKIqgEpz4yvTVnfjU/VbmL/r4yhwTTbPCNsZNi8g50/OnvbCsXu5wQqVURCBuOb7seu98n7A" +
           "/L23Tc8NX8mW6pL73UoOhYPH/GJv/I7Dzlqbg5pRUG1q++A//+Ng+4f9gDlATVzLHfErZiHioKrn" +
           "H37uhgeG597sdYnIYeeszypQqQawre9dHNbd0Yj9/5KnfsB8DJpuXXj8Q+ryj3dUZglD1Uz3MsWv" +
           "HX7uh1fv6QGHn7upAmrWQpEV2zSt+bVptamw+6C9VaP/hcoHrvkABgydUjPLywy6Oboh6HW6PgLj" +
           "LYqStqYRQHKDMQflMhXOQrnata27tvGvufrEn8ZBfmdPP2AO7NpmAAw85B8qTyjKlt1svAHTjPGL" +
           "k4w0jAcTAyllnBoh9Kxw/tEdS8cuT0WyH4vX1PYD5qMBzQDE2eFDxz09zsscWuwVHX6a8YwaFAiM" +
           "NAkHr4vdUdf82rQN6JwnSl4N4vAxeKdxP2A+mjXuKTvcXcY9TdOnyxPk4zKZ/vbRAqe75C3QfZZY" +
           "0P/y6/7299z+H4QrdGsoib8JAAAAAElFTkSuQmCC"
  }
};

// The process of adding a new default snippet involves:
//   * add a new entity to aboutHome.dtd
//   * add a <span/> for it in aboutHome.xhtml
//   * add an entry here in the proper ordering (based on spans)
// The <a/> part of the snippet will be linked to the corresponding url.
const DEFAULT_SNIPPETS_URLS = [
  "https://www.mozilla.org/firefox/features/?utm_source=snippet&utm_medium=snippet&utm_campaign=default+feature+snippet"
, "https://addons.mozilla.org/firefox/?utm_source=snippet&utm_medium=snippet&utm_campaign=addons"
];

const SNIPPETS_UPDATE_INTERVAL_MS = 86400000; // 1 Day.

// This global tracks if the page has been set up before, to prevent double inits
let gInitialized = false;
let gObserver = new MutationObserver(function (mutations) {
  for (let mutation of mutations) {
    if (mutation.attributeName == "searchEngineURL") {
      setupSearchEngine();
      if (!gInitialized) {
        ensureSnippetsMapThen(loadSnippets);
        gInitialized = true;
      }
      return;
    }
  }
});

window.addEventListener("pageshow", function () {
  // Delay search engine setup, cause browser.js::BrowserOnAboutPageLoad runs
  // later and may use asynchronous getters.
  window.gObserver.observe(document.documentElement, { attributes: true });
  fitToWidth();
  window.addEventListener("resize", fitToWidth);
});

window.addEventListener("pagehide", function() {
  window.gObserver.disconnect();
  window.removeEventListener("resize", fitToWidth);
});

// This object has the same interface as Map and is used to store and retrieve
// the snippets data.  It is lazily initialized by ensureSnippetsMapThen(), so
// be sure its callback returned before trying to use it.
let gSnippetsMap;
let gSnippetsMapCallbacks = [];

/**
 * Ensure the snippets map is properly initialized.
 *
 * @param aCallback
 *        Invoked once the map has been initialized, gets the map as argument.
 * @note Snippets should never directly manage the underlying storage, since
 *       it may change inadvertently.
 */
function ensureSnippetsMapThen(aCallback)
{
  if (gSnippetsMap) {
    aCallback(gSnippetsMap);
    return;
  }

  // Handle multiple requests during the async initialization.
  gSnippetsMapCallbacks.push(aCallback);
  if (gSnippetsMapCallbacks.length > 1) {
    // We are already updating, the callbacks will be invoked when done.
    return;
  }

  // TODO (bug 789348): use a real asynchronous storage here.  This setTimeout
  // is done just to catch bugs with the asynchronous behavior.
  setTimeout(function() {
    // Populate the cache from the persistent storage.
    let cache = new Map();
    for (let key of [ "snippets-last-update",
                      "snippets-cached-version",
                      "snippets" ]) {
      cache.set(key, localStorage[key]);
    }

    gSnippetsMap = Object.freeze({
      get: function (aKey) cache.get(aKey),
      set: function (aKey, aValue) {
        localStorage[aKey] = aValue;
        return cache.set(aKey, aValue);
      },
      has: function(aKey) cache.has(aKey),
      delete: function(aKey) {
        delete localStorage[aKey];
        return cache.delete(aKey);
      },
      clear: function() {
        localStorage.clear();
        return cache.clear();
      },
      get size() cache.size
    });

    for (let callback of gSnippetsMapCallbacks) {
      callback(gSnippetsMap);
    }
    gSnippetsMapCallbacks.length = 0;
  }, 0);
}

function onSearchSubmit(aEvent)
{
  let searchTerms = document.getElementById("searchText").value;
  let searchURL = document.documentElement.getAttribute("searchEngineURL");

  if (searchURL && searchTerms.length > 0) {
    const SEARCH_TOKENS = {
      "_searchTerms_": encodeURIComponent(searchTerms)
    }
    for (let key in SEARCH_TOKENS) {
      searchURL = searchURL.replace(key, SEARCH_TOKENS[key]);
    }

    // Send an event that a search was performed. This was originally
    // added so Firefox Health Report could record that a search from
    // about:home had occurred.
    let engineName = document.documentElement.getAttribute("searchEngineName");
    let event = new CustomEvent("AboutHomeSearchEvent", {detail: engineName});
    document.dispatchEvent(event);

    window.location.href = searchURL;
  }

  aEvent.preventDefault();
}


function setupSearchEngine()
{
  // The "autofocus" attribute doesn't focus the form element
  // immediately when the element is first drawn, so the
  // attribute is also used for styling when the page first loads.
  let searchText = document.getElementById("searchText");
  searchText.addEventListener("blur", function searchText_onBlur() {
    searchText.removeEventListener("blur", searchText_onBlur);
    searchText.removeAttribute("autofocus");
  });
 
  let searchEngineName = document.documentElement.getAttribute("searchEngineName");
  let searchEngineInfo = SEARCH_ENGINES[searchEngineName];
  let logoElt = document.getElementById("searchEngineLogo");

  // Add search engine logo.
  if (searchEngineInfo && searchEngineInfo.image) {
    logoElt.parentNode.hidden = false;
    logoElt.src = searchEngineInfo.image;
    logoElt.alt = searchEngineName;
    searchText.placeholder = "";
  }
  else {
    logoElt.parentNode.hidden = true;
    searchText.placeholder = searchEngineName;
  }

}

/**
 * Update the local snippets from the remote storage, then show them through
 * showSnippets.
 */
function loadSnippets()
{
  if (!gSnippetsMap)
    throw new Error("Snippets map has not properly been initialized");

  // Check cached snippets version.
  let cachedVersion = gSnippetsMap.get("snippets-cached-version") || 0;
  let currentVersion = document.documentElement.getAttribute("snippetsVersion");
  if (cachedVersion < currentVersion) {
    // The cached snippets are old and unsupported, restart from scratch.
    gSnippetsMap.clear();
  }

  // Check last snippets update.
  let lastUpdate = gSnippetsMap.get("snippets-last-update");
  let updateURL = document.documentElement.getAttribute("snippetsURL");
  let shouldUpdate = !lastUpdate ||
                     Date.now() - lastUpdate > SNIPPETS_UPDATE_INTERVAL_MS;
  if (updateURL && shouldUpdate) {
    // Try to update from network.
    let xhr = new XMLHttpRequest();
    try {
      xhr.open("GET", updateURL, true);
    } catch (ex) {
      showSnippets();
      return;
    }
    // Even if fetching should fail we don't want to spam the server, thus
    // set the last update time regardless its results.  Will retry tomorrow.
    gSnippetsMap.set("snippets-last-update", Date.now());
    xhr.onerror = function (event) {
      showSnippets();
    };
    xhr.onload = function (event)
    {
      if (xhr.status == 200) {
        gSnippetsMap.set("snippets", xhr.responseText);
        gSnippetsMap.set("snippets-cached-version", currentVersion);
      }
      showSnippets();
    };
    xhr.send(null);
  } else {
    showSnippets();
  }
}

/**
 * Shows locally cached remote snippets, or default ones when not available.
 *
 * @note: snippets should never invoke showSnippets(), or they may cause
 *        a "too much recursion" exception.
 */
let _snippetsShown = false;
function showSnippets()
{
  let snippetsElt = document.getElementById("snippets");

  // Show about:rights notification, if needed.
  let showRights = document.documentElement.getAttribute("showKnowYourRights");
  if (showRights) {
    let rightsElt = document.getElementById("rightsSnippet");
    let anchor = rightsElt.getElementsByTagName("a")[0];
    anchor.href = "about:rights";
    snippetsElt.appendChild(rightsElt);
    rightsElt.removeAttribute("hidden");
    return;
  }

  if (!gSnippetsMap)
    throw new Error("Snippets map has not properly been initialized");
  if (_snippetsShown) {
    // There's something wrong with the remote snippets, just in case fall back
    // to the default snippets.
    showDefaultSnippets();
    throw new Error("showSnippets should never be invoked multiple times");
  }
  _snippetsShown = true;

  let snippets = gSnippetsMap.get("snippets");
  // If there are remotely fetched snippets, try to to show them.
  if (snippets) {
    // Injecting snippets can throw if they're invalid XML.
    try {
      snippetsElt.innerHTML = snippets;
      // Scripts injected by innerHTML are inactive, so we have to relocate them
      // through DOM manipulation to activate their contents.
      Array.forEach(snippetsElt.getElementsByTagName("script"), function(elt) {
        let relocatedScript = document.createElement("script");
        relocatedScript.type = "text/javascript;version=1.8";
        relocatedScript.text = elt.text;
        elt.parentNode.replaceChild(relocatedScript, elt);
      });
      return;
    } catch (ex) {
      // Bad content, continue to show default snippets.
    }
  }

  showDefaultSnippets();
}

/**
 * Clear snippets element contents and show default snippets.
 */
function showDefaultSnippets()
{
  // Clear eventual contents...
  let snippetsElt = document.getElementById("snippets");
  snippetsElt.innerHTML = "";

  // ...then show default snippets.
  let defaultSnippetsElt = document.getElementById("defaultSnippets");
  let entries = defaultSnippetsElt.querySelectorAll("span");
  // Choose a random snippet.  Assume there is always at least one.
  let randIndex = Math.floor(Math.random() * entries.length);
  let entry = entries[randIndex];
  // Inject url in the eventual link.
  if (DEFAULT_SNIPPETS_URLS[randIndex]) {
    let links = entry.getElementsByTagName("a");
    // Default snippets can have only one link, otherwise something is messed
    // up in the translation.
    if (links.length == 1) {
      links[0].href = DEFAULT_SNIPPETS_URLS[randIndex];
    }
  }
  // Move the default snippet to the snippets element.
  snippetsElt.appendChild(entry);
}

function fitToWidth() {
  if (window.scrollMaxX) {
    document.body.setAttribute("narrow", "true");
  } else if (document.body.hasAttribute("narrow")) {
    document.body.removeAttribute("narrow");
    fitToWidth();
  }
}
